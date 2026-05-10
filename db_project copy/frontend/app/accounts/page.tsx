"use client";
import { useState, useMemo } from "react";
import { Users, TrendingUp, DollarSign } from "lucide-react";
import { PageWrapper } from "@/components/layout/PageWrapper";
import { Card } from "@/components/ui/Card";
import { StatWidget } from "@/components/ui/StatWidget";
import { ErrorCard } from "@/components/ui/ErrorCard";
import { SkeletonCard, SkeletonTable } from "@/components/ui/Skeleton";
import { BarChart } from "@/components/charts/BarChart";
import { DonutChart } from "@/components/charts/DonutChart";
import { useAccounts } from "@/hooks/useAccounts";
import { formatAmount } from "@/lib/utils";

export default function AccountsPage() {
  const { accounts, error, isLoading, mutate } = useAccounts();
  const [filter, setFilter] = useState("");
  const [sortAsc, setSortAsc] = useState(false);

  const entries = useMemo(() => {
    return Object.entries(accounts)
      .filter(([name]) => name.toLowerCase().includes(filter.toLowerCase()))
      .sort((a, b) => sortAsc ? a[1] - b[1] : b[1] - a[1]);
  }, [accounts, filter, sortAsc]);

  const totalAccounts = Object.keys(accounts).length;
  const totalBalance = Object.values(accounts).reduce((s, v) => s + v, 0);
  const richest = entries[0];

  const top15 = Object.entries(accounts).sort((a, b) => b[1] - a[1]).slice(0, 15)
    .map(([name, balance]) => ({ name, balance: parseFloat(balance.toFixed(2)) }));

  // All accounts as individual slices for the donut
  const donutData = Object.entries(accounts)
    .sort((a, b) => b[1] - a[1])
    .map(([name, balance]) => ({ name, value: parseFloat(balance.toFixed(2)) }))
    .filter(d => d.value > 0);

  const thStyle: React.CSSProperties = { padding: "10px 20px", textAlign: "left", fontSize: 11, fontWeight: 600, letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560", borderBottom: "1px solid #222222", fontFamily: "var(--font-body)" };
  const tdStyle: React.CSSProperties = { padding: "13px 20px", borderBottom: "1px solid #1a1a1a", fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)" };

  return (
    <PageWrapper title="Account Balances" subtitle="All accounts and their current balances" updatedAt={Date.now()}>
      {/* KPIs */}
      <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 16, marginBottom: 28 }} className="grid-3-col">
        {isLoading ? (
          Array.from({ length: 3 }).map((_, i) => <SkeletonCard key={i} rows={1} />)
        ) : (
          <>
            <StatWidget label="Total Accounts" value={totalAccounts} icon={<Users size={15} />} />
            <StatWidget label="Highest Balance" value={richest ? richest[0] : "—"} icon={<TrendingUp size={15} />} />
            <StatWidget label="Total Supply" value={formatAmount(totalBalance)} mono icon={<DollarSign size={15} />} />
          </>
        )}
      </div>

      {/* Charts */}
      {!isLoading && totalAccounts > 0 && (
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 20, marginBottom: 28 }} className="grid-2-col">
          <Card>
            <p style={{ fontSize: 13, fontWeight: 500, color: "#c9c3b8", marginBottom: 12, fontFamily: "var(--font-body)" }}>Top Accounts by Balance</p>
            <BarChart data={top15} xKey="name" yKey="balance" layout="horizontal" color="#c96442" formatter={v => formatAmount(Number(v))} height={Math.max(200, top15.length * 30)} />
          </Card>
          <Card>
            <p style={{ fontSize: 13, fontWeight: 500, color: "#c9c3b8", marginBottom: 12, fontFamily: "var(--font-body)" }}>Wealth Distribution</p>
            <DonutChart data={donutData} formatter={v => formatAmount(Number(v))} height={240} />
          </Card>
        </div>
      )}

      {/* Table */}
      <Card padding="none">
        <div style={{ padding: "14px 20px", borderBottom: "1px solid #222222", display: "flex", alignItems: "center", gap: 16 }}>
          <input
            placeholder="Filter by account name…"
            value={filter}
            onChange={e => setFilter(e.target.value)}
            style={{
              background: "#1a1a1a", border: "1px solid #2a2a2a", borderRadius: 8,
              color: "#f5f0e8", fontFamily: "var(--font-body)", fontSize: 14,
              padding: "8px 14px", outline: "none", width: 240,
            }}
          />
          <button
            onClick={() => setSortAsc(s => !s)}
            style={{
              marginLeft: "auto", background: "none", border: "none", cursor: "pointer",
              fontSize: 12, fontWeight: 600, letterSpacing: "0.08em", textTransform: "uppercase",
              color: "#6b6560", fontFamily: "var(--font-body)",
            }}
          >
            Balance {sortAsc ? "↑" : "↓"}
          </button>
        </div>

        {isLoading ? (
          <div style={{ padding: 24 }}><SkeletonTable rows={5} /></div>
        ) : error ? (
          <div style={{ padding: 24 }}><ErrorCard message={error?.message} onRetry={mutate} /></div>
        ) : (
          <div style={{ overflowX: "auto" }}>
            <table style={{ width: "100%", borderCollapse: "collapse" }}>
              <thead><tr>{["#", "Account", "Balance", "Share"].map(h => <th key={h} style={thStyle}>{h}</th>)}</tr></thead>
              <tbody>
                {entries.map(([name, balance], i) => (
                  <tr key={name}
                    onMouseEnter={e => (e.currentTarget as HTMLTableRowElement).style.background = "#1a1a1a"}
                    onMouseLeave={e => (e.currentTarget as HTMLTableRowElement).style.background = "transparent"}
                  >
                    <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13, color: "#6b6560", width: 48 }}>{i + 1}</td>
                    <td style={{ ...tdStyle, fontWeight: 500, color: "#f5f0e8" }}>{name}</td>
                    <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13, color: "#c9a842" }}>{formatAmount(balance)}</td>
                    <td style={tdStyle}>
                      <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
                        <div style={{ height: 6, borderRadius: 3, background: "#c96442", width: `${Math.max(4, (balance / totalBalance) * 120)}px`, maxWidth: 120 }} />
                        <span style={{ fontSize: 12, fontFamily: "var(--font-mono)", color: "#6b6560" }}>
                          {totalBalance > 0 ? ((balance / totalBalance) * 100).toFixed(1) : 0}%
                        </span>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </Card>
    </PageWrapper>
  );
}
