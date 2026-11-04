# -*- coding: utf-8 -*-
"""
NYBanner PC — приложение для управления новогодней LED-лентой (ESP32).

Три режима работы (вкладки):
  1) БЛОКИ (как Scratch)  — собираешь программу из цветных блоков, жмёшь «Запустить»,
     блоки исполняются по очереди и команды улетают на ESP32 по Wi-Fi.
  2) РЕДАКТОР             — просто пишешь текст, выбираешь эффект/скорость — «Показать».
  3) КОД                  — вводишь команды текстом (TEXT:, ADD:, EFFECT:, BRIGHT:, NEXT...)
     и отправляешь на ленту одной кнопкой.

Плюс: меню входа (подключение к ESP32), настройки ленты, сохранение/загрузка
проектов (.nyb), журнал ответов устройства.

Собрать EXE:  pyinstaller --noconsole --onefile --name NYBanner nybanner_pc.py
"""
import os
import sys
import json
import socket
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from urllib.parse import urlparse
from urllib.request import Request, urlopen

APP_VERSION = "3.0"
DEFAULT_IP = ""            # пусто = искать автоматически
API_PORT = 8765            # порт веб-API прошивки (HTTP_PORT в config.h)
SITE_URL = "https://github.com/ВАШ_ЛОГИН/nybanner/releases"  # <<< впишите ваш GitHub-логин

# ----------------------------- сетевой слой ----------------------------------
class BannerLink:
    """Общение с ESP32 по Wi-Fi (HTTP API прошивки)."""
    def __init__(self):
        self.host = None

    def set_host(self, ip):
        self.host = ip.strip() if ip and ip.strip() else None

    def _url(self, path):
        return f"http://{self.host}:{API_PORT}{path}"

    def command(self, cmd, timeout=4):
        """POST /api/command — та же обработка, что по Bluetooth."""
        req = Request(self._url("/api/command"), data=cmd.encode("utf-8"),
                      headers={"Content-Type": "text/plain"}, method="POST")
        with urllib_timeout(req, timeout) as r:
            return r.read().decode("utf-8", "replace").strip()

    def playlist(self, items, timeout=6):
        body = json.dumps(items, ensure_ascii=False).encode("utf-8")
        req = Request(self._url("/api/playlist"), data=body,
                      headers={"Content-Type": "application/json"}, method="POST")
        with urllib_timeout(req, timeout) as r:
            return r.read().decode("utf-8", "replace").strip()

    def status(self, timeout=3):
        req = Request(self._url("/api/status"))
        with urllib_timeout(req, timeout) as r:
            return r.read().decode("utf-8", "replace").strip()

    def send_prg(self, blocks_json, timeout=6):
        """POST /api/prg — загрузить Scratch-программу в память ленты и запустить."""
        body = blocks_json.encode("utf-8")
        req = Request(self._url("/api/prg"), data=body,
                      headers={"Content-Type": "application/json"}, method="POST")
        with urllib_timeout(req, timeout) as r:
            return r.read().decode("utf-8", "replace").strip()

    def get_prg(self, timeout=5):
        req = Request(self._url("/api/prg"))
        with urllib_timeout(req, timeout) as r:
            return r.read().decode("utf-8", "replace").strip()

    def run_prg(self, timeout=4):
        req = Request(self._url("/api/run"), data=b"", method="POST")
        with urllib_timeout(req, timeout) as r:
            return r.read().decode("utf-8", "replace").strip()

    def stop_prg(self, timeout=4):
        req = Request(self._url("/api/stop"), data=b"", method="POST")
        with urllib_timeout(req, timeout) as r:
            return r.read().decode("utf-8", "replace").strip()

    def reset_panel(self, timeout=6):
        """POST /api/reset — сбросить панель: лента покажет PIN-код."""
        req = Request(self._url("/api/reset"), data=b"", method="POST")
        with urllib_timeout(req, timeout) as r:
            return r.read().decode("utf-8", "replace").strip()

    def pair(self, code, timeout=4):
        body = json.dumps({"code": code}).encode("utf-8")
        req = Request(self._url("/api/pair"), data=body,
                      headers={"Content-Type": "application/json"}, method="POST")
        with urllib_timeout(req, timeout) as r:
            return r.read().decode("utf-8", "replace").strip()


def urllib_timeout(req, t):
    return urlopen(req, timeout=t)


def discover_devices(timeout=1.5):
    """UDP-ширококаст 'NYB_DISCOVER' — прошивка отвечает своим IP."""
    found = []
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        s.settimeout(timeout)
        s.sendto(b"NYB_DISCOVER", ("<broadcast>", 8766))
        while True:
            try:
                data, addr = s.recvfrom(1024)
            except socket.timeout:
                break
            txt = data.decode("utf-8", "replace")
            ip = addr[0]
            if ip not in found:
                found.append(ip)
        s.close()
    except OSError:
        pass
    return found

# ------------------------------- блоки-Scratch -------------------------------
# Типы блоков: (метка, категория, шаблон аргументов или None)
BLOCK_TYPES = [
    ("Показать текст",      "text",   'TEXT:{текст}',                 ["текст"]),
    ("Добавить в очередь",  "text",   'ADD:{текст}|{эффект}|{скор}',  ["текст", "эффект", "скорость"]),
    ("Эффект",              "look",   "EFFECT:{n}",                   ["эффект"]),
    ("Скорость",            "look",   "SPEED:{n}",                    ["число"]),
    ("Яркость",             "look",   "BRIGHT:{n}",                   ["число"]),
    ("Следующий клип",      "ctrl",   "NEXT",                         []),
    ("Предыдущий клип",     "ctrl",   "PREV",                         []),
    ("Очистить ленту",      "ctrl",   "CLEAR",                        []),
    ("Сохранить плейлист",  "ctrl",   "SAVE",                         []),
    ("Ждать (сек)",         "ctrl",   "WAIT:{сек}",                   ["сек"]),
    ("Мигнуть текстом N раз","text",  "BLINK:{текст}|{раз}",          ["текст", "раз"]),
    ("Новогодний салют",    "fx",     "FX:salute",                    []),
    ("Снег на ленте",       "fx",     "FX:snow",                      []),
    ("Ёлка на ленте",       "fx",     "FX:tree",                      []),
    ("Гирлянда (клип)",     "fx",     "FX:garland",                   []),
    ("Повторять программу", "ctrl",   "REPEAT",                       []),
]
# op для прошивки (ScratchVM) по метке блока
BLOCK_OPS = {
    "Показать текст": "text", "Добавить в очередь": "text", "Эффект": "effect",
    "Скорость": "speed", "Яркость": "bright", "Следующий клип": "next",
    "Предыдущий клип": "next", "Очистить ленту": "clear", "Сохранить плейлист": "clear",
    "Ждать (сек)": "wait", "Мигнуть текстом N раз": "blink",
    "Новогодний салют": "fx", "Снег на ленте": "fx", "Ёлка на ленте": "fx",
    "Гирлянда (клип)": "fx", "Повторять программу": "repeat",
}
BLOCK_FX = {"Новогодний салют": "salute", "Снег на ленте": "snow",
            "Ёлка на ленте": "tree", "Гирлянда (клип)": "garland"}
EFFECT_NAMES = {"0": "Белый", "1": "Гирлянда", "2": "Радуга", "3": "Снежный"}
CAT_COLORS = {"text": "#2f7bd9", "look": "#d9822f", "ctrl": "#7a4fd9", "fx": "#c2386b"}


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(f"NYBanner PC v{APP_VERSION} — управление LED-лентой")
        self.geometry("1000x680")
        self.link = BannerLink()
        self.log_lines = []
        self.running_blocks = False
        self.stop_flag = False
        self._build_ui()
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    # ------------------------------------------------------------------ UI ---
    def _build_ui(self):
        style = ttk.Style(self)
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass

        top = ttk.Frame(self, padding=6)
        top.pack(fill="x")
        ttk.Label(top, text="IP ленты:").pack(side="left")
        self.ip_var = tk.StringVar(value=DEFAULT_IP)
        ttk.Entry(top, textvariable=self.ip_var, width=18).pack(side="left", padx=4)
        ttk.Button(top, text="Подключиться", command=self.do_connect).pack(side="left", padx=2)
        ttk.Button(top, text="Найти авто", command=self.do_discover).pack(side="left", padx=2)
        ttk.Button(top, text="♻ Сбросить панель (код)", command=self.do_reset_panel).pack(side="left", padx=6)
        ttk.Button(top, text="🔑 PIN…", command=self.do_change_pin).pack(side="left", padx=2)
        self.status_lbl = ttk.Label(top, text="⚪ не подключено", foreground="#888")
        self.status_lbl.pack(side="right")

        self.nb = ttk.Notebook(self)
        self.nb.pack(fill="both", expand=True, padx=6, pady=4)
        self._tab_blocks()
        self._tab_editor()
        self._tab_code()
        self._tab_settings()

        bottom = ttk.Frame(self, padding=(6, 2))
        bottom.pack(fill="x")
        ttk.Label(bottom, text="Журнал:").pack(side="left")
        self.log = tk.Text(bottom, height=6, bg="#101418", fg="#9fe8a4",
                           insertbackground="#fff", font=("Consolas", 9))
        self.log.pack(fill="x", side="bottom", pady=2)

    def log_msg(self, msg):
        self.log.insert("end", msg + "\n")
        self.log.see("end")

    # ------------------------------------------------------- вкладка блоков --
    def _tab_blocks(self):
        fr = ttk.Frame(self.nb)
        self.nb.add(fr, text="🧩 Блоки (Scratch)")
        left = ttk.LabelFrame(fr, text="Палитра — двойной клик добавляет блок", padding=4)
        left.pack(side="left", fill="y", padx=4, pady=4)
        for label, cat, tmpl, args in BLOCK_TYPES:
            b = tk.Button(left, text=label, bg=CAT_COLORS[cat], fg="white",
                          activebackground=CAT_COLORS[cat], relief="flat", anchor="w",
                          font=("Segoe UI", 10, "bold"))
            b.pack(fill="x", pady=2)
            b.bind("<Double-Button-1>", lambda e, l=label, t=tmpl, a=args: self.add_block(l, t, a))

        mid = ttk.Frame(fr)
        mid.pack(side="left", fill="both", expand=True, padx=4)
        ctl = ttk.Frame(mid); ctl.pack(fill="x")
        ttk.Button(ctl, text="▶ Запустить", command=self.run_blocks).pack(side="left", padx=2)
        ttk.Button(ctl, text="⏹ Стоп", command=self.stop_blocks).pack(side="left", padx=2)
        ttk.Button(ctl, text="▲ Выше", command=lambda: self.move_block(-1)).pack(side="left", padx=2)
        ttk.Button(ctl, text="▼ Ниже", command=lambda: self.move_block(1)).pack(side="left", padx=2)
        ttk.Button(ctl, text="✕ Удалить", command=self.del_block).pack(side="left", padx=2)
        ttk.Button(ctl, text="💾 Сохранить .nyb", command=self.save_project).pack(side="left", padx=2)
        ttk.Button(ctl, text="📂 Открыть", command=self.open_project).pack(side="left", padx=2)
        ttk.Separator(ctl, orient="vertical").pack(side="left", fill="y", padx=4)
        ttk.Button(ctl, text="⬆ В ленту (autonomous)", command=self.upload_prg).pack(side="left", padx=2)
        ttk.Button(ctl, text="⏹ Стоп на ленте", command=self.stop_on_device).pack(side="left", padx=2)
        self.block_list = tk.Listbox(mid, font=("Segoe UI", 11), activestyle="dotbox")
        self.block_list.pack(fill="both", expand=True, pady=4)
        self.blocks = []  # список dict(label, tmpl, args=[(name,value)...])

    def add_block(self, label, tmpl, argnames):
        args = [(n, "") for n in argnames]
        if "эффект" in argnames:
            args[argnames.index("эффект")] = ("эффект", "1")
        if "скорость" in argnames:
            args[argnames.index("скорость")] = ("скорость", "2")
        if "сек" in argnames:
            args[argnames.index("сек")] = ("сек", "2")
        if "раз" in argnames:
            args[argnames.index("раз")] = ("раз", "3")
        self.blocks.append({"label": label, "tmpl": tmpl, "args": args})
        self._refresh_blocks()

    def _block_repr(self, i):
        b = self.blocks[i]
        parts = [f"{n}={v}" for n, v in b["args"] if v != ""]
        return f"{i+1}. {b['label']}" + (f"  [{', '.join(parts)}]" if parts else "")

    def _refresh_blocks(self):
        self.block_list.delete(0, "end")
        for i in range(len(self.blocks)):
            self.block_list.insert("end", self._block_repr(i))

    def edit_block(self, evt):
        sel = self.block_list.curselection()
        if not sel:
            return
        i = sel[0]; b = self.blocks[i]
        win = tk.Toplevel(self); win.title(b["label"]); win.geometry("360x220")
        entries = {}
        for j, (n, v) in enumerate(b["args"]):
            ttk.Label(win, text=n).grid(row=j, column=0, sticky="w", padx=8, pady=4)
            if n == "эффект":
                cb = ttk.Combobox(win, values=list(EFFECT_NAMES.values()), state="readonly")
                idx = max(0, min(3, int(v) if str(v).isdigit() else 1))
                cb.current(idx)
                cb.grid(row=j, column=1, padx=8, sticky="we"); entries[n] = cb
            else:
                e = ttk.Entry(win, width=30); e.insert(0, v)
                e.grid(row=j, column=1, padx=8, sticky="we"); entries[n] = e
        def ok():
            for j, (n, _) in enumerate(b["args"]):
                val = entries[n].get()
                if n == "эффект":
                    val = {v: k for k, v in EFFECT_NAMES.items()}.get(val, "1")
                b["args"][j] = (n, val)
            self._refresh_blocks(); win.destroy()
        ttk.Button(win, text="OK", command=ok).grid(row=len(b["args"]) + 1, column=1, sticky="e", padx=8, pady=8)
        win.transient(self)
    # связываем двойной клик по списку после создания block_list
        self.block_list.bind("<Double-Button-1>", self.edit_block)

    def del_block(self):
        sel = self.block_list.curselection()
        if sel:
            del self.blocks[sel[0]]; self._refresh_blocks()

    def move_block(self, d):
        sel = self.block_list.curselection()
        if not sel:
            return
        i = sel[0]; j = i + d
        if 0 <= j < len(self.blocks):
            self.blocks[i], self.blocks[j] = self.blocks[j], self.blocks[i]
            self._refresh_blocks(); self.block_list.selection_set(j)

    def save_project(self):
        p = filedialog.asksaveasfilename(defaultextension=".nyb",
                                         filetypes=[("NYBanner проект", "*.nyb")])
        if not p:
            return
        with open(p, "w", encoding="utf-8") as f:
            json.dump({"version": APP_VERSION, "blocks": [
                {"label": b["label"], "tmpl": b["tmpl"],
                 "args": [[n, v] for n, v in b["args"]]} for b in self.blocks]},
                f, ensure_ascii=False, indent=2)
        self.log_msg(f"проект сохранён: {p}")

    def open_project(self):
        p = filedialog.askopenfilename(filetypes=[("NYBanner проект", "*.nyb")])
        if not p:
            return
        try:
            with open(p, encoding="utf-8") as f:
                data = json.load(f)
            self.blocks = [{"label": b["label"], "tmpl": b["tmpl"],
                            "args": [tuple(a) for a in b["args"]]} for b in data.get("blocks", [])]
            self._refresh_blocks()
            self.log_msg(f"проект загружен: {p} ({len(self.blocks)} блоков)")
        except Exception as e:
            messagebox.showerror("Ошибка", f"Не удалось открыть файл:\n{e}")

    def compile_block(self, b):
        """Блок -> команда(ы) для ESP32. Возвращает список действий."""
        out = []
        vals = {n: v for n, v in b["args"]}
        cmd = b["tmpl"]
        for n, v in vals.items():
            cmd = cmd.replace("{" + n + "}", v)
        if cmd.startswith("WAIT:"):
            out.append(("wait", float(cmd.split(":")[1] or 0)))
        elif cmd.startswith("BLINK:"):
            rest = cmd[6:].split("|")
            txt = rest[0]; times = int(rest[1]) if len(rest) > 1 and rest[1].isdigit() else 3
            for _ in range(times):
                out.append(("cmd", f"TEXT:{txt}")); out.append(("wait", 1.0))
                out.append(("cmd", "CLEAR")); out.append(("wait", 0.4))
        else:
            out.append(("cmd", cmd))
        return out

    def run_blocks(self):
        if self.running_blocks:
            return
        if not self.link.host:
            messagebox.showwarning("Нет связи", "Сначала подключитесь к ленте (кнопка сверху).")
            return
        self.running_blocks = True; self.stop_flag = False
        threading.Thread(target=self._run_worker, daemon=True).start()

    def stop_blocks(self):
        self.stop_flag = True

    def _run_worker(self):
        try:
            for b in self.blocks:
                if self.stop_flag:
                    break
                self.log_line(f"▶ {b['label']}")
                for kind, val in self.compile_block(b):
                    if self.stop_flag:
                        break
                    if kind == "wait":
                        end = __import__("time").time() + val
                        while __import__("time").time() < end and not self.stop_flag:
                            __import__("time").sleep(0.1)
                    else:
                        try:
                            ans = self.link.command(val)
                            self.log_line(f"   → {val[:60]} ⇒ {ans[:60]}")
                        except Exception as e:
                            self.log_line(f"   ✗ ошибка связи: {e}")
                            self.set_status("🔴 связь потеряна", "#e57373")
                            self.running_blocks = False
                            return
        finally:
            self.running_blocks = False

    def log_line(self, s):
        self.log.insert("end", s + "\n"); self.log.see("end")

    # ---------------------------- Scratch-программа в памяти ленты ----------
    def compile_prg(self):
        """Список блоков -> JSON для прошивки (ScratchVM)."""
        arr = []
        for b in self.blocks:
            op = BLOCK_OPS.get(b["label"], "text")
            vals = {n: v for n, v in b["args"]}
            a = vals.get("текст", "") or vals.get("сек", "") or vals.get("число", "")
            if op == "fx":
                a = BLOCK_FX.get(b["label"], "salute")
            item = {"op": op, "a": a}
            if op == "text":
                eff = vals.get("эффект", "1")
                if not str(eff).isdigit():
                    eff = {v: k for k, v in EFFECT_NAMES.items()}.get(eff, "1")
                item["b"] = int(eff)
                item["c"] = int(vals.get("скорость", 2) or 2)
            if op == "blink":
                item["b"] = int(vals.get("раз", 3) or 3)
            arr.append(item)
        return json.dumps(arr, ensure_ascii=False)

    def upload_prg(self):
        if not self.link.host:
            messagebox.showwarning("Нет связи", "Сначала подключитесь к ленте.")
            return
        if not self.blocks:
            messagebox.showinfo("Пусто", "Добавьте блоки в программу.")
            return
        prg = self.compile_prg()
        def work():
            try:
                ans = self.link.send_prg(prg)
                self.log_line(f"⬆ программа из {len(self.blocks)} блоков загружена в ленту ⇒ {ans}")
                self.set_status("🟢 программа в ленте (работает автономно)", "#7ec97e")
            except Exception as e:
                self.log_line(f"✗ загрузка не удалась: {e}")
                self.set_status("🔴 связь потеряна", "#e57373")
        threading.Thread(target=work, daemon=True).start()

    def stop_on_device(self):
        def work():
            try:
                ans = self.link.stop_prg()
                self.log_line(f"⏹ лента остановила программу ⇒ {ans}")
            except Exception as e:
                self.log_line(f"✗ {e}")
        threading.Thread(target=work, daemon=True).start()

    def do_reset_panel(self):
        """Сбросить панель: лента покажет PIN-код и будет ждать привязки."""
        if not self.link.host:
            messagebox.showwarning("Нет связи", "Сначала подключитесь к ленте.")
            return
        if not messagebox.askyesno(
                "Сбросить панель?",
                "Лента покажет код (PIN) на панелях и перейдёт в режим привязки.\n"
                "Управление по Wi-Fi станет недоступно, пока вы не введёте код."
                "\n\nПродолжить?"):
            return
        def work():
            try:
                ans = self.link.reset_panel()
                self.log_line(f"♻ сброс панели ⇒ {ans}")
                self.set_status("🟡 панель ждёт ввода кода с ленты", "#ffd54f")
            except Exception as e:
                self.log_line(f"✗ сброс не удался: {e}")
        threading.Thread(target=work, daemon=True).start()

    def do_change_pin(self):
        """Сменить PIN-код ленты. 0000 = вход без пароля (открытый доступ)."""
        if not self.link.host:
            messagebox.showwarning("Нет связи", "Сначала подключитесь к ленте.")
            return
        win = tk.Toplevel(self)
        win.title("PIN-код ленты")
        win.geometry("360x170")
        ttk.Label(win, text="Новый PIN (4 цифры).\n0000 — открывает ленту БЕЗ пароля:",
                  justify="left").pack(padx=12, pady=(12, 4), anchor="w")
        var = tk.StringVar(value="0000")
        ent = ttk.Entry(win, textvariable=var, font=("Consolas", 16), width=8)
        ent.pack(pady=4)
        msg = ttk.Label(win, text="", foreground="#c62828")
        msg.pack()
        def ok():
            code = var.get().strip()
            if not (code.isdigit() and len(code) == 4):
                msg.config(text="Нужны ровно 4 цифры"); return
            def work():
                try:
                    ans = self.link.command("PIN " + code)
                    self.log_line(f"🔑 смена PIN ⇒ {ans}")
                    win.after(0, win.destroy)
                except Exception as e:
                    win.after(0, lambda: msg.config(text=str(e)))
            threading.Thread(target=work, daemon=True).start()
        ttk.Button(win, text="Применить", command=ok).pack(pady=6)
        ent.focus_set()

    # ------------------------------------------------------ вкладка редактор --
    def _tab_editor(self):
        fr = ttk.Frame(self.nb)
        self.nb.add(fr, text="✏️ Редактор")
        ttk.Label(fr, text="Текст для ленты:", font=("Segoe UI", 11, "bold")).pack(anchor="w", padx=8, pady=(8, 2))
        self.ed_text = ttk.Entry(fr, font=("Segoe UI", 14))
        self.ed_text.pack(fill="x", padx=8)
        row = ttk.Frame(fr); row.pack(fill="x", padx=8, pady=8)
        ttk.Label(row, text="Эффект:").pack(side="left")
        self.ed_eff = ttk.Combobox(row, values=list(EFFECT_NAMES.values()), width=12, state="readonly")
        self.ed_eff.current(1); self.ed_eff.pack(side="left", padx=4)
        ttk.Label(row, text="Скорость:").pack(side="left")
        self.ed_spd = ttk.Spinbox(row, from_=1, to=4, width=5); self.ed_spd.delete(0, "end"); self.ed_spd.insert(0, "2")
        self.ed_spd.pack(side="left", padx=4)
        ttk.Button(row, text="▶ Показать сейчас", command=self.editor_show).pack(side="left", padx=8)
        ttk.Button(row, text="+ В очередь ленты", command=self.editor_add).pack(side="left")
        ttk.Label(fr, text="Очередь (отправляется на ленту как плейлист):").pack(anchor="w", padx=8)
        self.ed_queue = tk.Listbox(fr, height=8, font=("Segoe UI", 11))
        self.ed_queue.pack(fill="both", expand=True, padx=8, pady=4)
        btm = ttk.Frame(fr); btm.pack(fill="x", padx=8, pady=4)
        ttk.Button(btm, text="📤 Отправить очередь на ленту", command=self.editor_send).pack(side="left")
        ttk.Button(btm, text="✕ Убрать выбранное", command=lambda: self.ed_queue.delete("active")).pack(side="left", padx=6)

    def editor_show(self):
        t = self.ed_text.get().strip()
        if not t:
            return
        eff = list(EFFECT_NAMES.keys())[self.ed_eff.current()]
        self._send_async(f"TEXT:{t}")
        self._send_async(f"EFFECT:{eff}")

    def editor_add(self):
        t = self.ed_text.get().strip()
        if not t:
            return
        eff = list(EFFECT_NAMES.keys())[self.ed_eff.current()]
        self.ed_queue.insert("end", f"{t}  [эфф.{eff}, скор.{self.ed_spd.get()}]")
        self.ed_text.delete(0, "end")

    def editor_send(self):
        items = []
        for line in self.ed_queue.get(0, "end"):
            txt = line.split("  [")[0]
            m = __import__("re").search(r"эфф\.(\d).*?скор\.(\d)", line)
            items.append({"t": txt, "e": int(m.group(1)) if m else 1,
                          "s": int(m.group(2)) if m else 2})
        if not items:
            messagebox.showinfo("Очередь пуста", "Добавьте тексты кнопкой «+ В очередь ленты».")
            return
        def work():
            try:
                ans = self.link.playlist(items)
                self.log_line(f"плейлист отправлен ⇒ {ans[:80]}")
            except Exception as e:
                self.log_line(f"✗ {e}")
        threading.Thread(target=work, daemon=True).start()

    # --------------------------------------------------------- вкладка код ---
    def _tab_code(self):
        fr = ttk.Frame(self.nb)
        self.nb.add(fr, text="⌨️ Код")
        ttk.Label(fr, text="Команды ESP32 (по одной на строку): TEXT:..., ADD:текст|1|2, EFFECT:2,"
                           " SPEED:3, BRIGHT:150, NEXT, PREV, CLEAR, SAVE, STATUS, FX:salute",
                  wraplength=900, justify="left").pack(anchor="w", padx=8, pady=6)
        self.code = tk.Text(fr, font=("Consolas", 12), bg="#0d1117", fg="#e6edf3",
                            insertbackground="#fff")
        self.code.pack(fill="both", expand=True, padx=8, pady=4)
        self.code.insert("end", "TEXT:С Новым Годом, школа!\nEFFECT:2\nBRIGHT:180\n")
        bar = ttk.Frame(fr); bar.pack(fill="x", padx=8, pady=6)
        ttk.Button(bar, text="🚀 Отправить всё на ленту", command=self.send_code).pack(side="left")
        ttk.Button(bar, text="Очистить", command=lambda: self.code.delete("1.0", "end")).pack(side="left", padx=6)

    def send_code(self):
        lines = [l.strip() for l in self.code.get("1.0", "end").splitlines() if l.strip()]
        if not lines:
            return
        def work():
            for ln in lines:
                try:
                    ans = self.link.command(ln)
                    self.log_line(f"{ln[:50]} ⇒ {ans[:50]}")
                except Exception as e:
                    self.log_line(f"✗ {ln[:30]} : {e}")
                    break
        threading.Thread(target=work, daemon=True).start()

    # ----------------------------------------------------- вкладка настройки --
    def _tab_settings(self):
        fr = ttk.Frame(self.nb)
        self.nb.add(fr, text="⚙️ Настройки ESP32")
        box = ttk.LabelFrame(fr, text="Wi-Fi ленты (сохраняется в прошивку)", padding=8)
        box.pack(fill="x", padx=8, pady=8)
        self.cfg_ssid = ttk.Entry(box, width=30); self.cfg_pass = ttk.Entry(box, width=30, show="*")
        ttk.Label(box, text="Имя сети Wi-Fi:").grid(row=0, column=0, sticky="w")
        self.cfg_ssid.grid(row=0, column=1, padx=6, pady=2)
        ttk.Label(box, text="Пароль:").grid(row=1, column=0, sticky="w")
        self.cfg_pass.grid(row=1, column=1, padx=6, pady=2)
        ttk.Button(box, text="💾 Применить (перезагрузит ленту)",
                   command=self.apply_wifi).grid(row=2, column=1, sticky="e", pady=6)
        info = ttk.LabelFrame(fr, text="Информация об устройстве", padding=8)
        info.pack(fill="x", padx=8)
        ttk.Button(info, text="🔄 Запросить статус", command=self.req_status).pack(anchor="w")
        self.info_lbl = ttk.Label(info, text="—", wraplength=700, justify="left")
        self.info_lbl.pack(anchor="w", pady=6)
        site = ttk.LabelFrame(fr, text="Приложения и файлы", padding=8)
        site.pack(fill="x", padx=8, pady=8)
        ttk.Button(site, text="🌐 Открыть страницу скачивания APK",
                   command=lambda: __import__("webbrowser").open(SITE_URL)).pack(anchor="w")

    def apply_wifi(self):
        ssid, pw = self.cfg_ssid.get(), self.cfg_pass.get()
        # прошивка принимает WIFI:ssid|pass через общий командный канал
        self._send_async(f"WIFI:{ssid}|{pw}")

    def req_status(self):
        def work():
            try:
                st = self.link.status()
                self.info_lbl.config(text=st)
            except Exception as e:
                self.info_lbl.config(text=f"нет связи: {e}")
        threading.Thread(target=work, daemon=True).start()

    # -------------------------------------------------------------- общее ---
    def _send_async(self, cmd):
        def work():
            try:
                ans = self.link.command(cmd)
                self.log_line(f"{cmd[:50]} ⇒ {ans[:50]}")
            except Exception as e:
                self.log_line(f"✗ {e}")
                self.set_status("🔴 нет связи", "#e57373")
        threading.Thread(target=work, daemon=True).start()

    def do_connect(self):
        ip = self.ip_var.get().strip()
        if not ip:
            self.do_discover(); return
        self.link.set_host(ip)
        def work():
            try:
                st = self.link.status()
                self.log_line(f"подключено к {ip}: {st[:80]}")
                self.set_status(f"🟢 {ip}", "#66bb6a")
            except Exception as e:
                self.log_line(f"✗ {ip}: {e}")
                self.set_status("🔴 недоступна", "#e57373")
        threading.Thread(target=work, daemon=True).start()

    def do_discover(self):
        self.log_line("искю ленты по сети (UDP)…")
        def work():
            devs = discover_devices()
            if devs:
                self.ip_var.set(devs[0]); self.link.set_host(devs[0])
                self.log_line(f"найдено: {', '.join(devs)}")
                self.set_status(f"🟢 {devs[0]}", "#66bb6a")
            else:
                self.log_line("ничего не найдено — введите IP вручную (см. монитор порта ESP32)")
        threading.Thread(target=work, daemon=True).start()

    def set_status(self, text, color):
        self.status_lbl.config(text=text, foreground=color)

    def on_close(self):
        self.stop_flag = True
        self.destroy()


def _crash_log(exc):
    """Пишем любую фатальную ошибку в nybanner_error.log рядом с программой."""
    try:
        import traceback, datetime, sys
        base = os.path.dirname(os.path.abspath(sys.argv[0]))
        p = os.path.join(base, "nybanner_error.log")
        with open(p, "a", encoding="utf-8") as f:
            f.write("\n===== %s =====\n" % datetime.datetime.now())
            traceback.print_exc(file=f)
        try:
            import tkinter.messagebox as mb
            r = tk.Tk(); r.withdraw()
            mb.showerror("NYBanner PC - oshibka",
                         "Programma upala. Tekst oshibki sohranen v:\n" + p)
            r.destroy()
        except Exception:
            pass
    except Exception:
        pass


if __name__ == "__main__":
    try:
        app = App()
        app.mainloop()
    except SystemExit:
        raise
    except BaseException as e:
        _crash_log(e)
        raise
