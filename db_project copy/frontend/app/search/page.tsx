"use client";
import { useState, useEffect } from "react";
import Link from "next/link";
import { Search } from "lucide-react";
import { PageWrapper } from "@/components/layout/PageWrapper";
import { Card } from "@/components/ui/Card";
import { Button } from "@/components/ui/Button";
import { HashDisplay } from "@/components/ui/HashDisplay";
import { EmptyState } from "@/components/ui/EmptyState";
import { ErrorCard } from "@/components/ui/ErrorCard";
import { useSearch, type SearchMode } from "@/hooks/useSearch";
import { formatAmount } from "@/lib/utils";

const MODES: { key: SearchMode; label: string }[] = [
  { key: "sender",   label: "By Sender" },
  { key: "receiver", label: "By Receiver" },
  { key: "txid",     label: "By TX ID" },
];
const STORAGE_KEY = "blockchain_recent_searches";
function getRecent(): string[] {
  try { return JSON.parse(sessionStorage.getItem(STORAGE_KEY) ?? "[]"); } catch { return []; }
}
function saveRecent(q: string) {
  const prev = getRecent().filter(s => s !== q);
  sessionStorage.setItem(STORAGE_KEY, JSON.stringify([q, ...prev].slice(0, 8)));
}

const thStyle: React.CSSProperties = {
  padding: "10px 20px", textAlign: "left", fontSize: 11, fontWeight: 600,
  letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560",
  borderBottom: "1px solid #222222", fontFamily: "var(--font-body)", whiteSpace: "nowrap",
};
const tdStyle: React.CSSProperties = {
  padding: "13px 20px", borderBottom: "1px solid #1a1a1a",
  fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)",
};
const rowStyle: React.CSSProperties = {
  display: "flex", alignItems: "center", justifyContent: "space-between",
  gap: 16, padding: "11px 0", borderBottom: "1px solid #1a1a1a",
};

export default function SearchPage() {
  const [mode, setMode] = useState<SearchMode>("sender");
  const [query, setQuery] = useState("");
  const [recent, setRecent] = useState<string[]>([]);
  const { search, loading, error, listResults, txResult } = useSearch();

  useEffect(() => { setRecent(getRecent()); }, []);

  const handleSearch = async (q = query) => {
    if (!q.trim()) return;
    saveRecent(q.trim());
    setRecent(getRecent());
    await search(mode, q.trim());
  };

  return (
    <PageWrapper title="Search" subtitle="Look up transactions by sender, receiver, or ID">
      {/* Search panel */}
      <Card style={{ marginBottom: 24 }}>
        {/* Mode toggle */}
        <div style={{ display: "flex", gap: 8, marginBottom: 20 }}>
          {MODES.map((m) => (
            <button key={m.key} onClick={() => setMode(m.key)} style={{
              padding: "7px 18px", borderRadius: 999, fontSize: 13, fontWeight: 500,
              fontFamily: "var(--font-body)", cursor: "pointer", transition: "all 150ms",
              background: mode === m.key ? "#c96442" : "#1a1a1a",
              color: mode === m.key ? "#fff" : "#c9c3b8",
              border: `1px solid ${mode === m.key ? "#c96442" : "#2a2a2a"}`,
            }}>
              {m.label}
            </button>
          ))}
        </div>

        {/* Input row */}
        <div style={{ display: "flex", gap: 12 }}>
          <div style={{ position: "relative", flex: 1 }}>
            <Search size={15} style={{ position: "absolute", left: 14, top: "50%", transform: "translateY(-50%)", color: "#6b6560" }} />
            <input
              placeholder={mode === "txid" ? "Enter transaction ID…" : `Enter ${mode} name…`}
              value={query}
              onChange={e => setQuery(e.target.value)}
              onKeyDown={e => e.key === "Enter" && handleSearch()}
              style={{
                background: "#1a1a1a", border: "1px solid #2a2a2a", borderRadius: 8,
                color: "#f5f0e8", fontFamily: "var(--font-body)", fontSize: 14,
                padding: "10px 14px 10px 40px", outline: "none", width: "100%",
              }}
              onFocus={e => (e.target as HTMLInputElement).style.borderColor = "rgba(201,100,66,0.5)"}
              onBlur={e => (e.target as HTMLInputElement).style.borderColor = "#2a2a2a"}
            />
          </div>
          <Button loading={loading} onClick={() => handleSearch()}>Search</Button>
        </div>

        {/* Recent searches */}
        {recent.length > 0 && (
          <div style={{ display: "flex", flexWrap: "wrap", gap: 8, marginTop: 14, alignItems: "center" }}>
            <span style={{ fontSize: 12, color: "#6b6560", fontFamily: "var(--font-body)" }}>Recent:</span>
            {recent.map(r => (
              <button key={r} onClick={() => { setQuery(r); handleSearch(r); }} style={{
                fontSize: 12, padding: "3px 12px", borderRadius: 999, cursor: "pointer",
                background: "#1a1a1a", color: "#c9c3b8", border: "1px solid #2a2a2a",
                fontFamily: "var(--font-body)", transition: "all 150ms",
              }}>
                {r}
              </button>
            ))}
          </div>
        )}
      </Card>

      {/* Error */}
      {error && <div style={{ marginBottom: 20 }}><ErrorCard message={error} /></div>}

      {/* TX ID single result */}
      {txResult && (
        txResult.found ? (
          <Card style={{ marginBottom: 20 }}>
            <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 18, color: "#f5f0e8", marginBottom: 20 }}>Transaction Found</h3>
            <dl style={{ display: "flex", flexDirection: "column" }}>
              {[
                { label: "TX ID",     value: <span style={{ fontFamily: "var(--font-mono)", color: "#c96442" }}>#{txResult.transaction.id}</span> },
                { label: "Sender",    value: txResult.transaction.sender },
                { label: "Receiver",  value: txResult.transaction.receiver },
                { label: "Amount",    value: <span style={{ fontFamily: "var(--font-mono)", color: "#c9a842" }}>{formatAmount(txResult.transaction.amount)}</span> },
                { label: "Metadata",  value: txResult.transaction.metadata || "—" },
                { label: "Signature", value: <HashDisplay hash={txResult.transaction.signature} /> },
                { label: "Block",     value: <Link href={`/blocks/${txResult.transaction.blockIndex}`} style={{ fontFamily: "var(--font-mono)", color: "#4278c9", textDecoration: "none" }}>#{txResult.transaction.blockIndex}</Link> },
              ].map(({ label, value }) => (
                <div key={label} style={rowStyle}>
                  <dt style={{ fontSize: 11, fontWeight: 600, letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560", fontFamily: "var(--font-body)", flexShrink: 0 }}>{label}</dt>
                  <dd style={{ fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)", textAlign: "right" }}>{value}</dd>
                </div>
              ))}
            </dl>
          </Card>
        ) : (
          <EmptyState title={`No transaction found for ID "${query}"`} />
        )
      )}

      {/* List results */}
      {listResults && (
        listResults.count === 0 ? (
          <EmptyState title={`No results found for "${query}"`} />
        ) : (
          <Card padding="none">
            <div style={{ padding: "16px 24px", borderBottom: "1px solid #222222" }}>
              <p style={{ fontSize: 14, fontWeight: 500, color: "#f5f0e8", fontFamily: "var(--font-body)", margin: 0 }}>
                {listResults.count} result{listResults.count !== 1 ? "s" : ""} for &ldquo;{query}&rdquo;
              </p>
            </div>
            <div style={{ overflowX: "auto" }}>
              <table style={{ width: "100%", borderCollapse: "collapse" }}>
                <thead><tr>{["TX ID", "Sender", "Receiver", "Amount", "Metadata", "Block"].map(h => <th key={h} style={thStyle}>{h}</th>)}</tr></thead>
                <tbody>
                  {listResults.results.map(tx => (
                    <tr key={tx.id}
                      onMouseEnter={e => (e.currentTarget as HTMLTableRowElement).style.background = "#1a1a1a"}
                      onMouseLeave={e => (e.currentTarget as HTMLTableRowElement).style.background = "transparent"}
                    >
                      <td style={tdStyle}><Link href={`/transactions/${tx.id}`} style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#c96442", textDecoration: "none" }}>#{tx.id}</Link></td>
                      <td style={tdStyle}>{tx.sender}</td>
                      <td style={tdStyle}>{tx.receiver}</td>
                      <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13, color: "#c9a842" }}>{formatAmount(tx.amount)}</td>
                      <td style={{ ...tdStyle, color: "#6b6560" }}>{tx.metadata || "—"}</td>
                      <td style={tdStyle}><Link href={`/blocks/${tx.blockIndex}`} style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#4278c9", textDecoration: "none" }}>#{tx.blockIndex}</Link></td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </Card>
        )
      )}
    </PageWrapper>
  );
}
