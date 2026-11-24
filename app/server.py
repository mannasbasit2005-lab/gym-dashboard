#!/usr/bin/env python3
"""
Gym Management System - web dashboard backend.

Uses only the Python standard library. Reads and writes the SAME text files
as the C program (members.txt and attendance.txt), so both tools see the
same data:

    members.txt     id|name|phone|plan|fee|payStatus|joinDate
    attendance.txt  id|DD-MM-YYYY
"""
import json
import os
import re
import tempfile
import threading
from datetime import date
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

DATA_DIR = os.environ.get("DATA_DIR", os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data"))
PORT = int(os.environ.get("PORT", "8080"))
MEMBER_FILE = os.path.join(DATA_DIR, "members.txt")
ATTENDANCE_FILE = os.path.join(DATA_DIR, "attendance.txt")
STATIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")

PLANS = {"Monthly": 3000.0, "Quarterly": 8000.0, "Yearly": 30000.0}  # same fees as the C program
LOCK = threading.Lock()


class ApiError(Exception):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status
        self.message = message


# ---------------------------------------------------------------- storage
def today():
    return date.today().strftime("%d-%m-%Y")


def write_atomic(path, text):
    """Write to a temp file, then rename, so a crash never leaves a half-written file."""
    fd, tmp = tempfile.mkstemp(dir=os.path.dirname(os.path.abspath(path)), prefix=".tmp-")
    with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    os.chmod(tmp, 0o666 & ~os.umask(0))  # keep the file usable by the C program's user too
    os.replace(tmp, path)


def save_members(members):
    lines = [
        f"{m['id']}|{m['name']}|{m['phone']}|{m['plan']}|{m['fee']:.2f}|{m['payStatus']}|{m['joinDate']}"
        for m in members
    ]
    write_atomic(MEMBER_FILE, "".join(l + "\n" for l in lines))


def load_members():
    if not os.path.exists(MEMBER_FILE):
        seed = [
            dict(id=1001, name="Ali Raza", phone="03001234567", plan="Monthly", fee=3000.0, payStatus="Paid", joinDate="01-11-2026"),
            dict(id=1002, name="Sara Khan", phone="03211234567", plan="Quarterly", fee=8000.0, payStatus="Unpaid", joinDate="05-11-2026"),
            dict(id=1003, name="Hamza Ahmed", phone="03331234567", plan="Yearly", fee=30000.0, payStatus="Paid", joinDate="10-11-2026"),
        ]
        save_members(seed)
        return seed
    members = []
    with open(MEMBER_FILE, encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\n").split("|")
            if len(parts) != 7:
                continue
            try:
                members.append(dict(id=int(parts[0]), name=parts[1], phone=parts[2], plan=parts[3],
                                    fee=float(parts[4]), payStatus=parts[5], joinDate=parts[6]))
            except ValueError:
                continue
    return members


def load_attendance():
    records = []
    if os.path.exists(ATTENDANCE_FILE):
        with open(ATTENDANCE_FILE, encoding="utf-8") as f:
            for line in f:
                parts = line.rstrip("\n").split("|")
                if len(parts) == 2:
                    try:
                        records.append((int(parts[0]), parts[1]))
                    except ValueError:
                        pass
    return records


def save_attendance(records):
    write_atomic(ATTENDANCE_FILE, "".join(f"{i}|{d}\n" for i, d in records))


# ------------------------------------------------------------- validation
def clean(value):
    return str(value or "").strip().replace("|", " ")


def check_name(name):
    name = clean(name)
    if not name:
        raise ApiError(400, "Name cannot be empty.")
    if len(name) > 49:
        raise ApiError(400, "Name is too long (maximum 49 characters).")
    return name


def check_phone(phone):
    phone = clean(phone)
    if len(phone) > 14:
        raise ApiError(400, "Phone is too long (maximum 14 characters).")
    if not re.fullmatch(r"[0-9+\-]{7,14}", phone):
        raise ApiError(400, "Phone must be 7-14 characters (digits, + or -).")
    return phone


def check_plan(plan):
    if plan not in PLANS:
        raise ApiError(400, "Plan must be Monthly, Quarterly or Yearly.")
    return plan


def find(members, member_id):
    for m in members:
        if m["id"] == member_id:
            return m
    raise ApiError(404, f"Member ID {member_id} not found.")


# ----------------------------------------------------------------- actions
def stats():
    members = load_members()
    ids = {m["id"] for m in members}
    present_ids = {i for i, d in load_attendance() if d == today()}
    paid = [m for m in members if m["payStatus"] == "Paid"]
    unpaid = [m for m in members if m["payStatus"] != "Paid"]
    return {
        "total": len(members), "paid": len(paid), "unpaid": len(unpaid),
        "collected": sum(m["fee"] for m in paid), "pending": sum(m["fee"] for m in unpaid),
        "presentToday": len(ids & present_ids),
        "plans": {p: sum(1 for m in members if m["plan"] == p) for p in PLANS},
        "today": today(), "planFees": PLANS,
    }


def list_members():
    members = load_members()
    present_ids = {i for i, d in load_attendance() if d == today()}
    return [dict(m, presentToday=m["id"] in present_ids) for m in members]


def add_member(body):
    members = load_members()
    if body.get("id") in (None, ""):
        member_id = max([m["id"] for m in members], default=1000) + 1
    else:
        try:
            member_id = int(body["id"])
        except (TypeError, ValueError):
            raise ApiError(400, "Invalid Member ID.")
        if member_id <= 0:
            raise ApiError(400, "Invalid Member ID.")
    if any(m["id"] == member_id for m in members):
        raise ApiError(409, f"Member ID {member_id} already exists!")
    plan = check_plan(body.get("plan"))
    member = dict(id=member_id, name=check_name(body.get("name")), phone=check_phone(body.get("phone")),
                  plan=plan, fee=PLANS[plan], payStatus="Unpaid", joinDate=today())
    members.append(member)
    save_members(members)
    return member


def update_member(member_id, body):
    members = load_members()
    m = find(members, member_id)
    if "name" in body:
        m["name"] = check_name(body["name"])
    if "phone" in body:
        m["phone"] = check_phone(body["phone"])
    save_members(members)
    return m


def delete_member(member_id):
    members = load_members()
    m = find(members, member_id)
    members.remove(m)
    save_members(members)
    save_attendance([r for r in load_attendance() if r[0] != member_id])
    return {"deleted": member_id}


def change_plan(member_id, body):
    members = load_members()
    m = find(members, member_id)
    m["plan"] = check_plan(body.get("plan"))
    m["fee"] = PLANS[m["plan"]]
    m["payStatus"] = "Unpaid"  # new plan = new fee due (same rule as the C program)
    save_members(members)
    return m


def set_payment(member_id, body):
    status = body.get("status")
    if status not in ("Paid", "Unpaid"):
        raise ApiError(400, "Status must be Paid or Unpaid.")
    members = load_members()
    m = find(members, member_id)
    m["payStatus"] = status
    save_members(members)
    return m


def mark_attendance(member_id):
    m = find(load_members(), member_id)
    records = load_attendance()
    if (member_id, today()) in records:
        raise ApiError(409, f"{m['name']} is already marked present for {today()}.")
    records.append((member_id, today()))
    save_attendance(records)
    return {"id": member_id, "name": m["name"], "date": today(), "unpaid": m["payStatus"] != "Paid"}


def attendance_history(member_id):
    m = find(load_members(), member_id)
    dates = [d for i, d in load_attendance() if i == member_id]
    return {"id": member_id, "name": m["name"], "dates": dates, "total": len(dates)}


def attendance_today():
    members = {m["id"]: m for m in load_members()}
    return [{"id": i, "name": members[i]["name"], "plan": members[i]["plan"], "payStatus": members[i]["payStatus"]}
            for i, d in load_attendance() if d == today() and i in members]


# ------------------------------------------------------------------ routing
class Handler(BaseHTTPRequestHandler):
    server_version = "GymDashboard/1.0"

    def log_message(self, fmt, *args):
        print("%s - %s" % (self.address_string(), fmt % args), flush=True)

    def send_json(self, status, payload):
        data = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def read_body(self):
        length = int(self.headers.get("Content-Length") or 0)
        if length > 10_000:
            raise ApiError(413, "Request too large.")
        if length == 0:
            return {}
        try:
            body = json.loads(self.rfile.read(length).decode("utf-8"))
        except (ValueError, UnicodeDecodeError):
            raise ApiError(400, "Request body must be valid JSON.")
        if not isinstance(body, dict):
            raise ApiError(400, "Request body must be a JSON object.")
        return body

    def handle_any(self, method):
        path = urlparse(self.path).path.rstrip("/") or "/"
        try:
            if method == "GET" and path in ("/", "/index.html"):
                with open(os.path.join(STATIC_DIR, "index.html"), "rb") as f:
                    data = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
                return
            if not path.startswith("/api/"):
                raise ApiError(404, "Not found.")
            body = self.read_body() if method in ("POST", "PUT") else {}
            with LOCK:
                status, payload = self.route(method, path, body)
            self.send_json(status, payload)
        except ApiError as e:
            self.send_json(e.status, {"error": e.message})
        except Exception as e:  # never leak a traceback to the browser
            print("Internal error:", repr(e), flush=True)
            self.send_json(500, {"error": "Internal server error."})

    def route(self, method, path, body):
        if method == "GET":
            if path == "/api/health":
                return 200, {"status": "ok"}
            if path == "/api/stats":
                return 200, stats()
            if path == "/api/members":
                return 200, list_members()
            if path == "/api/attendance/today":
                return 200, attendance_today()
            m = re.fullmatch(r"/api/attendance/(\d+)", path)
            if m:
                return 200, attendance_history(int(m.group(1)))
        if method == "POST" and path == "/api/members":
            return 201, add_member(body)
        m = re.fullmatch(r"/api/members/(\d+)(?:/(plan|payment|attendance))?", path)
        if m:
            member_id, action = int(m.group(1)), m.group(2)
            if method == "PUT" and action is None:
                return 200, update_member(member_id, body)
            if method == "DELETE" and action is None:
                return 200, delete_member(member_id)
            if method == "POST" and action == "plan":
                return 200, change_plan(member_id, body)
            if method == "POST" and action == "payment":
                return 200, set_payment(member_id, body)
            if method == "POST" and action == "attendance":
                return 201, mark_attendance(member_id)
        raise ApiError(404, "Not found.")

    def do_GET(self): self.handle_any("GET")
    def do_POST(self): self.handle_any("POST")
    def do_PUT(self): self.handle_any("PUT")
    def do_DELETE(self): self.handle_any("DELETE")


if __name__ == "__main__":
    os.makedirs(DATA_DIR, exist_ok=True)
    with LOCK:
        load_members()  # creates members.txt with sample data on first run
    print(f"Gym dashboard running on http://0.0.0.0:{PORT}  (data: {os.path.abspath(DATA_DIR)})", flush=True)
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
