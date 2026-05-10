# Blockchain Dashboard — Next.js Prompt

## Project Overview

Build a **production-grade, full-stack Blockchain Dashboard** using **Next.js (App Router)**. The visual design must precisely replicate the aesthetic of **Anthropic's website (anthropic.com)** — every color, font, spacing principle, and UI pattern must trace back to that reference.

The backend is a pre-built C++ API server (`blockchain_api.cpp`) that is **already written and fully running**. You have complete knowledge of its implementation — infer all request shapes, response structures, and TypeScript types directly from the endpoint definitions below. Do not define types speculatively; read the actual API responses during development and type them accurately. Every piece of data shown in the UI must come from a live API call.

---

## 1. Design System — Anthropic Theme (Strict)

### Color Palette

```css
:root {
  /* Backgrounds */
  --bg-primary:    #0a0a0a;   /* near-black page background */
  --bg-surface:    #111111;   /* card / panel surfaces */
  --bg-elevated:   #1a1a1a;   /* modals, dropdowns, hover states */
  --bg-border:     #222222;   /* subtle dividers */

  /* Cream / Off-white (Anthropic's signature warm neutral) */
  --cream:         #f5f0e8;   /* primary text on dark bg */
  --cream-muted:   #c9c3b8;   /* secondary / caption text */
  --cream-faint:   #6b6560;   /* tertiary / disabled text */

  /* Terracotta / Rust (Anthropic's brand accent) */
  --accent:        #c96442;
  --accent-hover:  #e07050;
  --accent-glow:   rgba(201, 100, 66, 0.15);

  /* Functional */
  --success:       #4a9e6b;
  --warning:       #c9a842;
  --danger:        #c94242;
  --info:          #4278c9;

  /* Chart palette — warm, earthy */
  --chart-1: #c96442;
  --chart-2: #c9a842;
  --chart-3: #4a9e6b;
  --chart-4: #4278c9;
  --chart-5: #9e4a8e;
}
```

### Typography
- **Heading font**: `"Styrene A"` → fallback `"DM Serif Display"` (Google Fonts)
- **Body / UI font**: `"Söhne"` → fallback `"DM Sans"` (Google Fonts)
- **Monospace** (hashes, IDs, numbers): `"Berkeley Mono"` → fallback `"JetBrains Mono"`
- Font scale (8pt grid): 12 / 14 / 16 / 20 / 24 / 32 / 48px
- Line-height: 1.6 body, 1.1 display. Letter-spacing: −0.02em headings, +0.08em caps labels

### Layout Principles
- Max content width: **1280px**, centered, 48px horizontal padding desktop / 24px mobile
- Generous whitespace — sections breathe, nothing cramped
- Navigation: fixed top bar, frosted glass (`backdrop-filter: blur(16px)`, `rgba(10,10,10,0.85)`)
- Cards: `border-radius: 12px`, `border: 1px solid var(--bg-border)`, `background: var(--bg-surface)`
- Buttons: pill (`border-radius: 9999px`) for primary CTAs; `border-radius: 8px` for secondary
- Transitions: 200ms ease-out on all hover states

---

## 2. Backend Integration — `blockchain_api.cpp`

### Base URL
```ts
// lib/constants.ts
export const API_BASE = process.env.NEXT_PUBLIC_API_BASE_URL ?? "http://localhost:8080";
```

### Actual API Endpoints

These are the exact endpoints exposed by `blockchain_api.cpp`. Infer all request body shapes and response structures from the C++ source — do not fabricate fields.

```
GET   /api/health                     DB + chain liveness status

GET   /api/blockchain/chains          List all saved chain names
POST  /api/blockchain/create          Body: { name, difficulty }
POST  /api/blockchain/load            Body: { name }
POST  /api/blockchain/save            Body: { name? }
GET   /api/blockchain/summary         Block count, latest hash, chain stats
GET   /api/blockchain/blocks          Full chain as JSON (all blocks)
GET   /api/blockchain/blocks/:index   Single block by index
GET   /api/blockchain/verify          Integrity check

GET   /api/accounts                   All account balances

POST  /api/transactions               Body: { sender, receiver, amount, metadata? }
GET   /api/transactions/pending       Priority queue of pending transactions

POST  /api/mine                       Mine all pending transactions into a new block

GET   /api/search/sender/:name        Transactions where sender matches
GET   /api/search/receiver/:name      Transactions where receiver matches
GET   /api/search/tx/:id              Single transaction by ID (BST lookup)
```

### TypeScript Types
**Do not pre-define types.** Instead:
1. Call each endpoint during development and `console.log` the raw JSON response
2. Derive accurate interfaces in `lib/types.ts` from the actual shapes returned
3. Mark nullable/optional fields with `?` or `| null` as observed
4. Use `unknown` temporarily if uncertain; narrow before using in UI

### Data Fetching Strategy
- **Next.js Server Components** for initial SSR data loads
- **SWR** on the client for polling and mutations
- Polling intervals: 5s for health and pending txs; 15s for blocks and accounts
- All mutations (`POST /api/mine`, `POST /api/transactions`, etc.) call SWR `mutate()` to revalidate affected queries after success
- Every API call must have an error state — show a styled "API Unavailable" card, never a crash
- Show shimmer skeleton loaders while fetching; never blank pages

---

## 3. Project Structure

```
/app
  layout.tsx
  page.tsx                      ← redirect to /dashboard
  /dashboard/page.tsx           ← Overview
  /blocks/page.tsx              ← Block Explorer list
  /blocks/[index]/page.tsx      ← Single block detail
  /transactions/page.tsx        ← Transaction Ledger + new tx form
  /transactions/[id]/page.tsx   ← Single transaction detail
  /accounts/page.tsx            ← Account Balances
  /mine/page.tsx                ← Mining Control Panel
  /search/page.tsx              ← Universal Search
  /chains/page.tsx              ← Chain Management

/components
  /layout
    Navbar.tsx
    PageWrapper.tsx
    Footer.tsx
  /ui
    Card.tsx
    Badge.tsx
    Button.tsx
    StatWidget.tsx
    HashDisplay.tsx
    Skeleton.tsx
    Tooltip.tsx
    EmptyState.tsx
    ErrorCard.tsx
    Toast.tsx
  /charts
    LineChart.tsx
    BarChart.tsx
    AreaChart.tsx
    DonutChart.tsx
  /blocks
    BlockTable.tsx
    BlockCard.tsx
  /transactions
    TxTable.tsx
    TxForm.tsx
  /accounts
    AccountTable.tsx
  /mine
    MineButton.tsx
    PendingTxList.tsx
  /search
    SearchBar.tsx
    SearchResults.tsx
  /chains
    ChainList.tsx
    ChainForm.tsx

/lib
  api.ts          ← typed fetch wrappers for every endpoint
  types.ts        ← inferred from actual API responses
  utils.ts        ← formatHash, formatAmount, timeAgo, formatNumber
  constants.ts

/hooks
  useHealth.ts
  useChain.ts
  useBlocks.ts
  useTransactions.ts
  useAccounts.ts
  useMine.ts
  useSearch.ts

/styles
  globals.css
```

---

## 4. Dashboard Tabs — Feature Specs

All data in every tab comes exclusively from the API endpoints above. No simulated, seeded, or hardcoded values.

---

### Tab 1: Overview `/dashboard`

**Purpose**: Bird's-eye health of the currently loaded chain.

**Data sources**: `GET /api/health`, `GET /api/blockchain/summary`, `GET /api/transactions/pending`

**KPI Row** (StatWidgets):
- System health status from `/api/health`
- Total block count from `/api/blockchain/summary`
- Latest block hash (truncated, copyable)
- Pending transaction count from `/api/transactions/pending`
- Chain integrity status from `GET /api/blockchain/verify`

**Charts** (derived by iterating `/api/blockchain/blocks`):
- **Bar chart** — transactions per block (x = block index, y = tx count inside that block)
- **Area chart** — cumulative transaction volume over block height (walk each block's transactions, accumulate total `amount` transferred)
- **Donut chart** — blocks with at least one transaction vs empty blocks

**Live feed** (refreshes every 10s):
- Latest 10 blocks: Index | Hash | Tx count | Timestamp
- Latest 5 pending transactions from the priority queue

---

### Tab 2: Block Explorer `/blocks` + `/blocks/[index]`

**Purpose**: Browse and inspect every block on the chain.

**Data sources**: `GET /api/blockchain/blocks`, `GET /api/blockchain/blocks/:index`, `GET /api/blockchain/verify`

**Block list page**:
- Paginated table: Index | Hash | Timestamp | # Transactions | (any additional fields from response)
- Client-side pagination over the full chain array
- Click any row → navigate to `/blocks/[index]`
- "Verify Chain Integrity" button → `GET /api/blockchain/verify` → result banner (green valid / red compromised)

**Block detail page** (`/blocks/[index]`):
- Render all fields from `GET /api/blockchain/blocks/:index`
- List all transactions inside the block; each TX ID links to `/transactions/[id]`
- Previous / Next block navigation buttons

**Visualization**: mini sparkline bar chart of tx-count-per-block across the current page

---

### Tab 3: Transaction Ledger `/transactions` + `/transactions/[id]`

**Purpose**: View all confirmed transactions and submit new ones.

**Data sources**: `GET /api/blockchain/blocks` (walk all blocks for confirmed txs), `POST /api/transactions`, `GET /api/search/tx/:id`

**Transaction list**:
- Walk all blocks from `GET /api/blockchain/blocks`, flatten every transaction into one table
- Columns: TX ID | Sender | Receiver | Amount | Metadata | Block Index
- Click any row → navigate to `/transactions/[id]`

**New Transaction panel** (slide-in sidebar or modal):
```
Sender:    [text input]
Receiver:  [text input]
Amount:    [number input]
Metadata:  [optional text input]
           [Submit] → POST /api/transactions
```
- Success: toast notification + refresh pending count
- Error: inline error with API message

**Transaction detail page** (`/transactions/[id]`):
- Fetch via `GET /api/search/tx/:id`
- Display all fields; link to the containing block

**Charts**:
- **Horizontal bar chart** — top 10 senders by total amount sent
- **Horizontal bar chart** — top 10 receivers by total amount received

---

### Tab 4: Mining Control Panel `/mine`

**Purpose**: Real-time mempool view and one-click block mining.

**Data sources**: `GET /api/transactions/pending`, `POST /api/mine`

**Layout**:
- Left panel — live pending transactions list (poll every 5s):
  - Columns: TX ID | Sender | Receiver | Amount | any priority fields from response
  - Empty state: "Mempool is clear — no pending transactions"
- Right panel — mining:
  - Prominent **"Mine Block"** button (pill, `var(--accent)`)
  - On click: `POST /api/mine` → button shows loading + "Mining…" → on success, banner "Block mined successfully" + revalidate blocks and pending
  - Session mine log: list of block indices mined in this browser session (React state)

**Charts**:
- **Area chart** — pending transaction count over time (rolling window sampled every 5s in client state)
- **Bar chart** — amount distribution of current pending txs (bucketed ranges)

---

### Tab 5: Account Balances `/accounts`

**Purpose**: View balances for every account on the chain.

**Data source**: `GET /api/accounts`

**Layout**:
- KPI widgets: total account count | highest balance account | sum of all balances
- Table: Account Name | Balance | (any other fields from response)
- Default sort: balance descending; toggleable
- Filter input to search by account name client-side

**Charts**:
- **Horizontal bar chart** — top 15 accounts by balance
- **Donut chart** — wealth distribution: top 5 accounts vs everyone else

---

### Tab 6: Search `/search`

**Purpose**: Universal lookup across senders, receivers, and transaction IDs.

**Data sources**: `GET /api/search/sender/:name`, `GET /api/search/receiver/:name`, `GET /api/search/tx/:id`

**Layout**:
- Prominent search input
- Mode toggle: **By Sender** | **By Receiver** | **By TX ID**
- Results:
  - Sender / Receiver mode → transaction table matching the name
  - TX ID mode → single transaction detail card
- Empty state: "No results found for '{query}'"
- Recent searches stored in `sessionStorage`

---

### Tab 7: Chain Management `/chains`

**Purpose**: Create, load, save, and verify multiple chain instances.

**Data sources**: `GET /api/blockchain/chains`, `POST /api/blockchain/create`, `POST /api/blockchain/load`, `POST /api/blockchain/save`, `GET /api/blockchain/verify`

**Layout**:
- **Saved Chains** list from `GET /api/blockchain/chains`
  - Each row: chain name | [Load] button → `POST /api/blockchain/load`
  - "Active" badge on whichever chain is currently loaded
- **Create New Chain** form:
  ```
  Chain Name:  [text input]
  Difficulty:  [number input]
               [Create] → POST /api/blockchain/create
  ```
- **Save Current Chain** — `POST /api/blockchain/save` with optional name override
- **Verify Integrity** — `GET /api/blockchain/verify` → result card (green "Valid ✓" / red "Compromised ✗" with details from response)

All actions: loading state on button + success/error toast on completion

---

## 5. Navigation

### Top Navbar
```
[⬡ {Chain Name}]   Overview  Blocks  Transactions  Mine  Accounts  Search  Chains   [● LIVE]
```
- Chain name from `GET /api/blockchain/summary`; fallback "Dashboard" if no chain loaded
- Active link: 2px underline `var(--accent)`, no background
- `● LIVE` indicator: pulsing green when last health poll passed; amber when stale (>15s); red when `/api/health` errors
- Frosted glass: `background: rgba(10,10,10,0.85); backdrop-filter: blur(16px); border-bottom: 1px solid var(--bg-border)`
- Mobile: hamburger → full-screen slide-in drawer

### Page Header (every tab)
```
[H1: Page Title]                              [Last updated: 12s ago]
[One-line subtitle describing the tab]
──────────────────────────────────────────────────────────────────────
```

### Footer
```
Health: {status}  ·  Chain: {name}  ·  Blocks: {count}          © {year} Blockchain Dashboard
```

---

## 6. Reusable Component Specs

### `StatWidget`
- Background `var(--bg-surface)`, border `var(--bg-border)`
- Label: 11px all-caps, `var(--cream-faint)`, letter-spacing 0.08em
- Value: 28–32px, `var(--cream)`, monospace
- Optional delta badge: pill, 15% opacity green/red background

### `HashDisplay`
- Shows first 10 + `…` + last 6 chars
- Hover tooltip: full string
- Clipboard icon; ✓ confirmation for 2s after copy

### `Badge`
```tsx
<Badge variant="success">Valid</Badge>
<Badge variant="danger">Compromised</Badge>
<Badge variant="warning">Pending</Badge>
<Badge variant="info">Loaded</Badge>
```
Pill shape, 12px text, icon optional

### Chart Wrappers (Recharts)
- All colors from CSS variables — no hardcoded hex inside chart configs
- Custom tooltip: `background: var(--bg-elevated)`, `border-left: 3px solid var(--accent)`, cream text
- Grid: horizontal dotted lines only, `stroke: var(--bg-border)`, `strokeDasharray: 4 4`
- Smooth curves: `type="monotone"` on Line/Area
- Each chart: title above, axis labels, last-updated timestamp below in muted text

### `ErrorCard`
- Icon + "Unable to load data" + API error message
- "Retry" button re-triggers the SWR fetch

### `Skeleton`
- CSS keyframe shimmer: `var(--bg-surface)` → `var(--bg-elevated)` → `var(--bg-surface)`
- Same dimensions as the component it replaces

---

## 7. Anthropic UI Micro-Details

1. Section dividers: `1px solid var(--bg-border)`, full width
2. Table row hover: background → `var(--bg-elevated)` in 150ms
3. Input focus ring: `2px solid rgba(201,100,66,0.4)`, no default outline
4. Scrollbars: `scrollbar-width: thin`, thumb `var(--bg-elevated)`, track transparent
5. Text selection: `background: var(--accent-glow); color: var(--cream)`
6. Toast notifications: slide in from bottom-right, same card styling, auto-dismiss after 4s
7. Loading buttons: label replaced with spinner + "Processing…", pointer-events disabled

---

## 8. Quality Requirements

- **No mock data** — every value on screen comes from `blockchain_api.cpp`
- **TypeScript strict mode** — all types derived from actual API responses; zero `any`
- **SSR** initial render for all pages via Next.js Server Components
- **Error boundaries** on every tab — never an unhandled crash
- **Responsive**: 375px / 768px / 1280px+ fully functional
- **Accessibility**: keyboard nav, ARIA labels on icon-only buttons, status never conveyed by color alone
- **Single config point**: `API_BASE` in `lib/constants.ts`
- **README.md**: how to start the C++ server, how to run Next.js dev server, all env vars explained

---

## 9. Dependencies

```json
{
  "dependencies": {
    "next": "^14",
    "react": "^18",
    "react-dom": "^18",
    "recharts": "^2",
    "swr": "^2",
    "clsx": "^2",
    "date-fns": "^3",
    "lucide-react": "^0.383"
  },
  "devDependencies": {
    "typescript": "^5",
    "@types/react": "^18",
    "tailwindcss": "^3",
    "autoprefixer": "^10",
    "postcss": "^8"
  }
}
```

> Use Tailwind **only** for layout utilities (grid, flex, spacing, responsive prefixes). All color, typography, and theme values live in `globals.css` CSS variables. Never use Tailwind color classes.

---

*Paste this entire document into Claude Code, Cursor, or Windsurf as your project specification.*
