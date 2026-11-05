#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
sender.py — ПК-приложение «Новогодняя лента» (Windows / macOS / Linux)

Что делает:
  1. Берёт ваши видео/картинки (mp4, avi, mov, gif, png, jpg), нарезает кадры,
     конвертирует в формат ленты (.bin из RGB565-кадров) и складывает в ./data_out/
  2. Поднимает локальный HTTP-сервер (порт 8765), с которого ESP32 сама качает
     файлы по Wi-Fi (по одному, фоном — «предзагрузка следующего клипа»).
  3. Шлёт команды на ESP32: PLAY / PAUSE / NEXT / RELOAD / STATUS (Wi-Fi HTTP).
  4. Умеет Bluetooth LE (опционально): подключается к "NY-Feed-ESP32" и пишет
     те же команды в UART-характеристику. Нужна библиотека bleak:  pip install bleak
  5. GUI (tkinter, без доп. зависимостей): список клипов, кнопки управления,
     прогресс закачки на ESP32, поле IP ленты.

Запуск:
    python sender.py gui
    python sender.py convert my_video.mp4 --fps 5 --width 160
    python sender.py serve            # только файл-сервер
    python sender.py send play --ip 192.168.1.42
    python sender.py bt play          # через Bluetooth
    python sender.py make-fonts       # генерация русских шрифтов .vlw в data_out/fonts

Зависимости для конвертации видео:  pip install pillow   (+ ffmpeg в PATH для mp4/avi)
"""
import argparse
import hashlib
import http.server
import json
import os
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import urllib.parse
import urllib.request
from pathlib import Path

DATA_DIR = Path(__file__).resolve().parent / "data_out"
FILE_PORT = 8765          # порт, с которого ESP32 качает файлы
ESP_DEFAULT_PORT = 80     # HTTP-API ESP32

VIDEO_EXTS = {".mp4", ".avi", ".mov", ".mkv", ".webm"}
IMAGE_EXTS = {".png", ".jpg", ".jpeg", ".bmp", ".gif"}


# --------------------------------------------------------------------------
#  Формат клипа .bin:  [fw u16][fh u16] + N раз: [px RGB565 big-endian]
# --------------------------------------------------------------------------
def rgb565_be(r, g, b):
    v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
    return struct.pack(">H", v)


def frame_to_bin(img) -> bytes:
    """PIL.Image -> строка RGB565 big-endian."""
    img = img.convert("RGB")
    w, h = img.size
    px = img.load()
    out = bytearray(w * h * 2)
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            out[i] = v >> 8
            out[i + 1] = v & 0xFF
            i += 2
    return bytes(out)


def extract_frames(path: Path, fps: int, width: int):
    """Генератор PIL-кадров из видео (через ffmpeg) или картинки/gif."""
    try:
        from PIL import Image
    except ImportError:
        sys.exit("Нужна Pillow:  pip install pillow")

    ext = path.suffix.lower()
    if ext in VIDEO_EXTS:
        tmpdir = tempfile.mkdtemp(prefix="nyfeed_")
        pattern = os.path.join(tmpdir, "f%06d.png")
        cmd = ["ffmpeg", "-y", "-i", str(path), "-vf", f"fps={fps},scale={width}:-1", pattern]
        try:
            subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except FileNotFoundError:
            sys.exit("Не найден ffmpeg. Установите его (https://ffmpeg.org) или используйте картинки.")
        frames = sorted(Path(tmpdir).glob("f*.png"))
        for f in frames:
            yield Image.open(f)
    elif ext in IMAGE_EXTS:
        img = Image.open(path)
        if getattr(img, "n_frames", 1) > 1:      # анимированный gif
            for i in range(img.n_frames):
                img.seek(i)
                yield img.resize((width, max(1, int(img.height * width / img.width))))
        else:
            yield img.resize((width, max(1, int(img.height * width / img.width))))
    else:
        sys.exit(f"Неизвестный формат: {path}")


def sha1short_head(data: bytes) -> str:
    """Хэш первых 4 КБ — ровно то же, что сверяет downloader.h прошивки."""
    return hashlib.sha1(data[:4096]).hexdigest()[:8]


def build_clip(src: Path, fps: int, width: int) -> dict:
    """Видео/картинка -> clip_XX.bin + мета для index.json."""
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    idx = len(list(DATA_DIR.glob("clip_*.bin")))
    cid = f"clip_{idx:02d}"
    bin_path = DATA_DIR / f"{cid}.bin"

    fw = fh = 0
    nframes = 0
    with open(bin_path, "wb") as out:
        header_written = False
        for fr in extract_frames(src, fps, width):
            blob = frame_to_bin(fr)
            fw, fh = fr.size
            if not header_written:
                out.write(struct.pack(">HH", fw, fh))
                header_written = True
            out.write(blob)
            nframes += 1
        out.flush()

    size = bin_path.stat().st_size
    digest = sha1short_head(bin_path.read_bytes()[:4096])
    meta = {"id": cid, "file": f"/data/{cid}.bin", "fw": fw, "fh": fh,
            "fps": fps, "frames": nframes, "bytes": size, "sha1short": digest}
    print(f"[OK] {src.name} -> {bin_path.name}: {nframes} кадров, {size} байт")
    return meta


def rebuild_index():
    """index.json по всем .bin в data_out (с повторным чтением заголовков)."""
    clips = []
    for p in sorted(DATA_DIR.glob("clip_*.bin")):
        with open(p, "rb") as f:
            hdr = f.read(4)
            if len(hdr) < 4:
                continue
            fw, fh = struct.unpack(">HH", hdr)
        size = p.stat().st_size
        frames = (size - 4) // (fw * fh * 2)
        digest = sha1short_head(p.read_bytes()[:4096])
        clips.append({"id": p.stem, "file": f"/data/{p.name}", "fw": fw, "fh": fh,
                      "fps": 5, "frames": frames, "bytes": size, "sha1short": digest})
    DATA_DIR.mkdir(exist_ok=True)
    (DATA_DIR / "index.json").write_text(json.dumps({"clips": clips}, ensure_ascii=False), "utf-8")
    print(f"[OK] index.json: {len(clips)} клипов")
    return clips


# --------------------------------------------------------------------------
#  Файловый сервер (отдаёт ./data_out по HTTP, порт 8765)
# --------------------------------------------------------------------------
class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=str(DATA_DIR), **kw)

    def log_message(self, fmt, *args):
        print("[serve]", fmt % args)


def serve_files(block=True):
    httpd = http.server.ThreadingHTTPServer(("0.0.0.0", FILE_PORT), QuietHandler)
    print(f"Файлы ленты: http://{get_local_ip()}:{FILE_PORT}/  (папка {DATA_DIR})")
    if block:
        httpd.serve_forever()
    else:
        threading.Thread(target=httpd.serve_forever, daemon=True).start()
    return httpd


def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


# --------------------------------------------------------------------------
#  Команды ESP32 по Wi-Fi (HTTP API прошивки)
# --------------------------------------------------------------------------
def esp_command(ip: str, cmd: str) -> str:
    cmd = cmd.lower()
    url = f"http://{ip}:{ESP_DEFAULT_PORT}/{cmd}"
    req = urllib.request.Request(url, data=b"", method="POST")
    with urllib.request.urlopen(req, timeout=5) as r:
        return r.read().decode()


def esp_setup(ip: str, host: str):
    """Сказать ESP32, где лежат файлы (IP этого ПК)."""
    url = f"http://{ip}:{ESP_DEFAULT_PORT}/setup"
    data = urllib.parse.urlencode({"host": host}).encode()
    req = urllib.request.Request(url, data=data, method="POST")
    with urllib.request.urlopen(req, timeout=5) as r:
        return r.read().decode()


def esp_status(ip: str) -> str:
    url = f"http://{ip}:{ESP_DEFAULT_PORT}/"
    with urllib.request.urlopen(url, timeout=5) as r:
        html = r.read().decode("utf-8", "ignore")
    return html.split("<pre>")[1].split("</pre>")[0] if "<pre>" in html else html


# --------------------------------------------------------------------------
#  Bluetooth LE (опционально, pip install bleak)
# --------------------------------------------------------------------------
NUS_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
BT_NAME = "NY-Feed-ESP32"


async def bt_send_async(cmd: str):
    from bleak import BleakClient, BleakScanner
    dev = await BleakScanner.find_device_by_name(BT_NAME, timeout=10)
    if not dev:
        raise RuntimeError(f"Bluetooth-устройство '{BT_NAME}' не найдено")
    async with BleakClient(dev) as client:
        await client.write_gatt_char(NUS_RX, cmd.encode(), response=False)
    print(f"[BT] отправлено: {cmd}")


def bt_send(cmd: str):
    import asyncio
    asyncio.run(bt_send_async(cmd))


# --------------------------------------------------------------------------
#  Русские шрифты .vlw (для loadFont в прошивке)
# --------------------------------------------------------------------------
def make_fonts():
    """Генерирует data_out/fonts/Cyrillic_Arial14.vlw — русский шрифт для TFT.
    Требуется PIL и любой TTF с кириллицой (берём системный Arial/DejaVu)."""
    from PIL import Image, ImageDraw, ImageFont
    candidates = ["C:/Windows/Fonts/arial.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                  "/System/Library/Fonts/Supplemental/Arial.ttf"]
    ttf = next((c for c in candidates if os.path.exists(c)), None)
    if not ttf:
        sys.exit("Не найден TTF-шрифт с кириллицей")
    size = 14
    font = ImageFont.truetype(ttf, size)
    chars = [chr(c) for c in range(0x20, 0x70)] + [chr(c) for c in range(0x410, 0x450)]
    first, count = 0x20, 0
    bitmap = bytearray()
    adv = bytearray()
    glyph_w = size + 2
    for ch in chars:
        code = ord(ch)
        if code - first >= 0x100:
            continue
        while code - first > count:
            adv.append(0); bitmap.extend(b"\x00" * ((glyph_w + 7) // 8 * (size + 2))); count += 1
        img = Image.new("L", (glyph_w, size + 2), 0)
        ImageDraw.Draw(img).text((0, 0), ch, fill=255, font=font)
        adv.append(glyph_w)
        row_bytes = (glyph_w + 7) // 8
        for y in range(size + 2):
            for xb in range(row_bytes):
                byte = 0
                for bit in range(8):
                    x = xb * 8 + bit
                    if x < glyph_w and img.getpixel((x, y)):
                        byte |= 0x80 >> bit
                bitmap.append(byte)
        count += 1
    DATA_DIR.mkdir(exist_ok=True)
    (DATA_DIR / "fonts").mkdir(exist_ok=True)
    vlw = DATA_DIR / "fonts" / "Cyrillic_Arial14.vlw"
    with open(vlw, "wb") as f:
        f.write(struct.pack(">BBHHH", 0x75, 0xf9, first, count, size))
        f.write(bytes(adv)); f.write(bytes(bitmap))
    print(f"[OK] шрифт: {vlw} ({vlw.stat().st_size} байт). Загрузите его на ESP32 кнопкой 'Шрифт->ESP' в GUI.")


# --------------------------------------------------------------------------
#  GUI (tkinter)
# --------------------------------------------------------------------------
def run_gui():
    import tkinter as tk
    from tkinter import filedialog, messagebox, scrolledtext

    win = tk.Tk(); win.title("Новогодняя лента — пульт ESP32"); win.geometry("640x520")

    tk.Label(win, text="IP ESP32:").grid(row=0, column=0, sticky="e", padx=4, pady=4)
    ip_var = tk.StringVar(value="192.168.1.1")
    tk.Entry(win, textvariable=ip_var, width=18).grid(row=0, column=1, sticky="w")

    status = scrolledtext.ScrolledText(win, height=10)
    status.grid(row=5, column=0, columnspan=4, sticky="nsew", padx=6, pady=6)
    win.columnconfigure(3, weight=1); win.rowconfigure(5, weight=1)

    listbox = tk.Listbox(win, height=8, width=60)
    listbox.grid(row=1, column=0, columnspan=4, sticky="nsew", padx=6)

    def log(msg):
        status.insert("end", msg + "\n"); status.see("end")

    def refresh_list():
        listbox.delete(0, "end")
        idx = DATA_DIR / "index.json"
        if idx.exists():
            data = json.loads(idx.read_text("utf-8"))
            for c in data.get("clips", []):
                listbox.insert("end", f'{c["id"]}  {c["frames"]} кадров  {c["bytes"]} Б')

    def add_files():
        files = filedialog.askopenfilenames(
            title="Выберите видео/картинки",
            filetypes=[("Медиа", "*.mp4 *.avi *.mov *.mkv *.webm *.gif *.png *.jpg *.jpeg *.bmp")])
        for fn in files:
            log(f"Конвертирую {os.path.basename(fn)} ...")
            try:
                build_clip(Path(fn), fps=5, width=160)
            except SystemExit as e:
                log(str(e))
        rebuild_index(); refresh_list()

    def do_esp(fn, name):
        ip = ip_var.get().strip()
        try:
            log(f"[{name}] {fn(ip)}")
        except Exception as e:
            log(f"[{name}] ошибка: {e}  (проверьте IP и что лента в той же Wi-Fi сети)")

    def connect_pc():
        ip = ip_var.get().strip()
        serve_files(block=False)
        try:
            log("setup: " + esp_setup(ip, get_local_ip()))
        except Exception as e:
            log(f"setup失败: {e}")

    row = tk.Frame(win); row.grid(row=4, column=0, columnspan=4, pady=4)
    for label, cmd in [("Play", "play"), ("Pause", "pause"), ("Next", "next"), ("Reload", "reload")]:
        tk.Button(row, text=label, command=lambda c=cmd: do_esp(lambda ip, c=c: esp_command(ip, c), c)).pack(side="left", padx=3)
    tk.Button(row, text="Статус", command=lambda: do_esp(esp_status, "status")).pack(side="left", padx=3)
    tk.Button(row, text="Подключить ПК->ленту", command=connect_pc).pack(side="left", padx=3)

    row2 = tk.Frame(win); row2.grid(row=3, column=0, columnspan=4, pady=2)
    tk.Button(row2, text="Добавить видео/фото", command=add_files).pack(side="left", padx=3)
    tk.Button(row2, text="Обновить список", command=lambda: (rebuild_index(), refresh_list())).pack(side="left", padx=3)
    tk.Button(row2, text="BT: Play", command=lambda: bt_send_safe("PLAY")).pack(side="left", padx=3)
    tk.Button(row2, text="BT: Next", command=lambda: bt_send_safe("NEXT")).pack(side="left", padx=3)
    tk.Button(row2, text="Русский шрифт", command=lambda: do(make_fonts, "fonts")).pack(side="left", padx=3)

    def bt_send_safe(cmd):
        try:
            bt_send(cmd)
        except ImportError:
            log("Bluetooth: установите bleak ->  pip install bleak")
        except Exception as e:
            log(f"BT ошибка: {e}")

    def do(fn, name):
        try:
            fn()
        except SystemExit as e:
            log(f"[{name}] {e}")

    refresh_list()
    serve_files(block=False)
    win.mainloop()


# --------------------------------------------------------------------------
#  CLI
# --------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="NY Feed sender")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("convert"); p.add_argument("file"); p.add_argument("--fps", type=int, default=5)
    p.add_argument("--width", type=int, default=160)
    sub.add_parser("index")
    sp = sub.add_parser("send"); sp.add_argument("command"); sp.add_argument("--ip", required=True)
    bp = sub.add_parser("bt"); bp.add_argument("command")
    up = sub.add_parser("setup"); up.add_argument("--ip", required=True)
    st = sub.add_parser("status"); st.add_argument("--ip", required=True)
    sub.add_parser("serve")
    sub.add_parser("make-fonts")
    sub.add_parser("gui")
    a = ap.parse_args()

    if a.cmd == "convert":
        build_clip(Path(a.file), a.fps, a.width); rebuild_index()
    elif a.cmd == "index":
        rebuild_index()
    elif a.cmd == "send":
        print(esp_command(a.ip, a.command))
    elif a.cmd == "bt":
        bt_send(a.command)
    elif a.cmd == "setup":
        serve_files(block=False); print(esp_setup(a.ip, get_local_ip())); input("Ctrl+C чтобы остановить...")
    elif a.cmd == "status":
        print(esp_status(a.ip))
    elif a.cmd == "serve":
        serve_files()
    elif a.cmd == "make-fonts":
        make_fonts()
    elif a.cmd == "gui":
        run_gui()


if __name__ == "__main__":
    main()
