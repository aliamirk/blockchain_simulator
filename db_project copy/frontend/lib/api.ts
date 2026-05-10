import { API_BASE } from "./constants";
import type {
  HealthResponse,
  BlocksResponse,
  Block,
  SummaryResponse,
  VerifyResponse,
  PendingResponse,
  AccountsResponse,
  ChainsResponse,
  CreateChainResponse,
  LoadChainResponse,
  SaveChainResponse,
  AddTransactionResponse,
  MineResponse,
  SearchTxResponse,
  SearchListResponse,
} from "./types";

function friendlyNetworkError(err: unknown, url: string): Error {
  const raw = err instanceof Error ? err.message : String(err);

  // Browser "Failed to fetch" / Node "fetch failed" — server not reachable
  if (/failed to fetch|fetch failed|network request failed/i.test(raw)) {
    return new Error(
      `Cannot reach the API server at ${API_BASE}.\n` +
      `Make sure the C++ backend is running:\n` +
      `  ./blockchain_api   (default port 8080)\n` +
      `Then refresh this page.`
    );
  }

  // DNS / connection refused
  if (/ECONNREFUSED|ENOTFOUND|ERR_CONNECTION_REFUSED/i.test(raw)) {
    return new Error(
      `Connection refused on ${API_BASE}.\n` +
      `The API server is not listening on that address/port.`
    );
  }

  // CORS
  if (/cors/i.test(raw)) {
    return new Error(
      `CORS error — the browser blocked the request to ${API_BASE}.\n` +
      `Check that the C++ server sends Access-Control-Allow-Origin headers.`
    );
  }

  return new Error(`Network error on ${url}: ${raw}`);
}

async function apiFetch<T>(path: string, init?: RequestInit): Promise<T> {
  const url = `${API_BASE}${path}`;
  let res: Response;

  try {
    res = await fetch(url, {
      ...init,
      headers: { "Content-Type": "application/json", ...(init?.headers ?? {}) },
    });
  } catch (err) {
    throw friendlyNetworkError(err, url);
  }

  // Try to parse JSON; fall back to plain text for non-JSON error bodies
  let json: unknown;
  try {
    json = await res.json();
  } catch {
    const text = await res.text().catch(() => "");
    throw new Error(`HTTP ${res.status} ${res.statusText}${text ? ` — ${text.slice(0, 200)}` : ""}`);
  }

  if (!res.ok) {
    const apiMsg = (json as { error?: string })?.error;
    throw new Error(apiMsg ?? `HTTP ${res.status} ${res.statusText}`);
  }

  return json as T;
}

// ── Health ────────────────────────────────────────────────────────────────────
export const fetchHealth = () =>
  apiFetch<HealthResponse>("/api/health");

// ── Blockchain ────────────────────────────────────────────────────────────────
export const fetchChains = () =>
  apiFetch<ChainsResponse>("/api/blockchain/chains");

export const fetchSummary = () =>
  apiFetch<SummaryResponse>("/api/blockchain/summary");

export const fetchBlocks = () =>
  apiFetch<BlocksResponse>("/api/blockchain/blocks");

export const fetchBlock = (index: number) =>
  apiFetch<Block>(`/api/blockchain/blocks/${index}`);

export const fetchVerify = () =>
  apiFetch<VerifyResponse>("/api/blockchain/verify");

export const createChain = (name: string, difficulty: number) =>
  apiFetch<CreateChainResponse>("/api/blockchain/create", {
    method: "POST",
    body: JSON.stringify({ name, difficulty }),
  });

export const loadChain = (name: string) =>
  apiFetch<LoadChainResponse>("/api/blockchain/load", {
    method: "POST",
    body: JSON.stringify({ name }),
  });

export const saveChain = (name?: string) =>
  apiFetch<SaveChainResponse>("/api/blockchain/save", {
    method: "POST",
    body: JSON.stringify(name ? { name } : {}),
  });

// ── Accounts ──────────────────────────────────────────────────────────────────
export const fetchAccounts = () =>
  apiFetch<AccountsResponse>("/api/accounts");

// ── Transactions ──────────────────────────────────────────────────────────────
export const fetchPending = () =>
  apiFetch<PendingResponse>("/api/transactions/pending");

export const addTransaction = (
  sender: string,
  receiver: string,
  amount: number,
  metadata?: string
) =>
  apiFetch<AddTransactionResponse>("/api/transactions", {
    method: "POST",
    body: JSON.stringify({ sender, receiver, amount, metadata }),
  });

// ── Mine ──────────────────────────────────────────────────────────────────────
export const mineBlock = () =>
  apiFetch<MineResponse>("/api/mine", { method: "POST" });

// ── Search ────────────────────────────────────────────────────────────────────
export const searchBySender = (name: string) =>
  apiFetch<SearchListResponse>(`/api/search/sender/${encodeURIComponent(name)}`);

export const searchByReceiver = (name: string) =>
  apiFetch<SearchListResponse>(`/api/search/receiver/${encodeURIComponent(name)}`);

export const searchByTxId = (id: number) =>
  apiFetch<SearchTxResponse>(`/api/search/tx/${id}`);

// ── SWR fetcher ───────────────────────────────────────────────────────────────
export const swrFetcher = (url: string) => {
  const fullUrl = `${API_BASE}${url}`;
  return fetch(fullUrl)
    .then((r) => {
      if (!r.ok) throw new Error(`HTTP ${r.status} ${r.statusText}`);
      return r.json();
    })
    .catch((err) => {
      // Re-throw already-friendly errors unchanged
      if (err instanceof Error && err.message.startsWith("Cannot reach")) throw err;
      throw friendlyNetworkError(err, fullUrl);
    });
};
