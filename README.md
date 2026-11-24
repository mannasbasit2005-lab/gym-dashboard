# Gym Management System – Web Dashboard

A browser dashboard for the Gym Management System C project (Iqra University, M9 Campus).
It shows live figures, manages members, plans, payments and attendance, and runs in Docker with one command.

The dashboard saves to the **same two text files as the C program** (`members.txt`, `attendance.txt`), so the
web page and the original console program always work on the same data.

```
gym-dashboard/
├── app/
│   ├── server.py          Python backend (standard library only, no pip install)
│   └── static/index.html  The dashboard page (no external CDN, works offline)
├── cli/
│   └── gym_management_system.c   The original C program
├── data/                  members.txt + attendance.txt are stored here
├── Dockerfile             Dashboard image
├── Dockerfile.cli         Optional image for the C console program
└── docker-compose.yml
```

## Run with Docker (recommended)

```bash
docker compose up -d --build
```

Open **http://localhost:8080**. On the first start three sample members are created automatically.

| Task | Command |
|---|---|
| View logs | `docker compose logs -f dashboard` |
| Stop | `docker compose down` |
| Rebuild after editing code | `docker compose up -d --build` |
| Run the original C program on the same data | `docker compose run --rm cli` |

Your data stays in the `./data` folder on your computer, so it survives container restarts and rebuilds.
To back it up, copy `data/members.txt` and `data/attendance.txt`.

**Linux note:** the containers run as user ID 1000. If you see a permission error, run
`sudo chown -R 1000:1000 data` (or `chmod 777 data`). Docker Desktop on Windows/macOS needs nothing.

## Run without Docker

Needs only Python 3.8+:

```bash
python3 app/server.py
```

Then open http://localhost:8080. Optional environment variables:

| Variable | Default | Meaning |
|---|---|---|
| `PORT` | `8080` | Port to listen on |
| `DATA_DIR` | `./data` | Folder holding `members.txt` and `attendance.txt` |

To use a different host port with Docker, change `"8080:8080"` in `docker-compose.yml` (for example `"9000:8080"`).

## What the dashboard does

- **Overview cards:** total members, present today, paid, unpaid, fees collected, fees pending, plus a payment-status chart and members-per-plan bars.
- **Members:** search by ID, name or phone; filter by plan and payment; add, edit, delete (with confirmation); change plan; click the payment badge to toggle Paid/Unpaid; mark attendance.
- **Payments:** unpaid and paid lists with one-click *Record payment* / *New billing cycle*.
- **Attendance:** who is present today, and the full attendance history of any member.

The rules are identical to the C program: a new member starts **Unpaid**; changing a plan updates the fee and resets
payment to **Unpaid**; attendance is allowed once per member per day; deleting a member also deletes their attendance;
phone numbers are 7–14 characters of digits, `+` or `-`; the `|` character is replaced by a space.

## Plan fees

Monthly PKR 3,000 · Quarterly PKR 8,000 · Yearly PKR 30,000.
To change them, edit `PLANS` in `app/server.py` **and** the `FEE_*` values at the top of `cli/gym_management_system.c`
so both programs agree, then rebuild.

## Using the dashboard and the C program together

Both can use the same `data` folder, but the C program loads its records into memory when it starts and writes that copy
back when you save/exit. So **use one at a time**: close the C program before using the dashboard (and restart the C program
after dashboard changes). If both change data at the same time, the one that saves last wins.

## REST API

All endpoints return JSON; errors look like `{"error": "message"}`.

| Method & path | Purpose |
|---|---|
| `GET /api/health` | Health check |
| `GET /api/stats` | Totals, fees collected/pending, present today, members per plan |
| `GET /api/members` | All members (with `presentToday`) |
| `POST /api/members` | Register `{name, phone, plan, id?}` (ID auto-assigned if omitted) |
| `PUT /api/members/<id>` | Update `{name?, phone?}` |
| `DELETE /api/members/<id>` | Delete member and their attendance |
| `POST /api/members/<id>/plan` | Change plan `{plan}` (resets payment to Unpaid) |
| `POST /api/members/<id>/payment` | Set payment `{status: "Paid" or "Unpaid"}` |
| `POST /api/members/<id>/attendance` | Mark present today |
| `GET /api/attendance/today` | Members present today |
| `GET /api/attendance/<id>` | Attendance history of one member |

## Security note

The dashboard has **no login**. Run it on a trusted computer or network only. Do not expose port 8080 to the internet
unless you put it behind a reverse proxy with authentication (for example Caddy, Nginx or Traefik with basic auth or SSO).
