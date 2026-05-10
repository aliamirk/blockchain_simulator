# Blockchain Dashboard

A production-grade Next.js dashboard for the C++ blockchain API server.

## Prerequisites

- Node.js 18+
- The C++ API server compiled and running (see below)
- MySQL running with `blockchain_db` schema initialized

---

## 1. Start the C++ API Server

```bash
# From the project root (db_project copy/)

# First-time DB setup (run once):
mysql -u root -p < init.sql

# Compile:
make api

# Run (default port 8080):
./blockchain_api

# Custom port:
PORT=9000 ./blockchain_api
```

### DB environment variables (all optional — defaults shown)

| Variable       | Default       | Description              |
|----------------|---------------|--------------------------|
| `DB_HOST`      | `127.0.0.1`   | MySQL host               |
| `DB_PORT`      | `3306`        | MySQL port               |
| `DB_NAME`      | `blockchain_db` | Database name          |
| `DB_USER`      | `root`        | MySQL username           |
| `DB_PASS`      | `p4pacific`   | MySQL password           |
| `PORT`         | `8080`        | HTTP server port         |

---

## 2. Start the Next.js Dev Server

```bash
# From this directory (frontend/)
npm install
npm run dev
```

Open [http://localhost:3000](http://localhost:3000) — it redirects to `/dashboard`.

### Frontend environment variables

Create a `.env.local` file if the API runs on a different host/port:

```env
NEXT_PUBLIC_API_BASE_URL=http://localhost:8080
```

---

## 3. Production Build

```bash
npm run build
npm start
```

---

## Pages

| Route | Description |
|-------|-------------|
| `/dashboard` | Overview — KPIs, charts, live feeds |
| `/blocks` | Block explorer with pagination |
| `/blocks/[index]` | Single block detail |
| `/transactions` | Transaction ledger + new TX form |
| `/transactions/[id]` | Single transaction detail |
| `/accounts` | Account balances with charts |
| `/mine` | Mining control panel |
| `/search` | Universal search (sender / receiver / TX ID) |
| `/chains` | Chain management (create / load / save / verify) |

---

## Tech Stack

- **Next.js 16** (App Router, React 19)
- **SWR** — data fetching with polling
- **Recharts** — charts
- **Tailwind CSS v4** — layout utilities only
- **CSS variables** — all theming (Anthropic-inspired dark palette)
- **lucide-react** — icons
