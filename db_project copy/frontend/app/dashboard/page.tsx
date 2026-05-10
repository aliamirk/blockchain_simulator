"use client";
import { useState } from "react";
import Link from "next/link";
import { Activity, Blocks, Clock, Zap, ShieldCheck } from "lucide-react";
import { PageWrapper } from "@/components/layout/PageWrapper";
import { StatWidget } from "@/components/ui/StatWidget";
import { Card } from "@/components/ui/Card";
import { Badge } from "@/components/ui/Badge";
import { ErrorCard } from "@/components/ui/ErrorCard";
import { SkeletonCard } from "@/components/ui/Skeleton";
import { HashDisplay } from "@/components/ui/HashDisplay";
import { BarChart } from "@/components/charts/BarChart";
import { AreaChart } from "@/components/charts/AreaChart";
import { DonutChart } from "@/components/charts/DonutChart";
import { useHealth } from "@/hooks/useHealth";
import { useSummary, useVerify } from "@/hooks/useChain";
import { useBlocks } from "@/hooks/useBlocks";
import { usePending } from "@/hooks/useTransactions";
import { formatAmount, timeAgo } from "@/lib/utils";

export default function DashboardPage() {
  const [updatedAt] = useState(Date.now());
  const { health, error: healthErr, isLoading: healthLoading, mutate: retryHealth } = useHealth();
  const { summary, isLoading: summaryLoading } = useSummary();
  const { verify, isLoading: verifyLoading } = useVerify();
  const { blocks, error: blocksErr, isLoading: blocksLoading } = useBlocks();
  const { pending, count: pendingCount } = usePending();

  // Chart data
  const txPerBlock = blocks.map((b) => ({ block: `#${b.index}`, txs: b.transactions.length }));
  const cumulativeVolume = blocks.reduce<{ block: string; volume: number }[]>((acc, b) => {
    const prev = acc[acc.length - 1]?.volume ?? 0;
    const sum = b.transactions.reduce((s, t) => s + t.amount, 0);
    acc.push({ block: `#${b.index}`, volume: parseFloat((prev + sum).toFixed(2)) });
    return acc;
  }, []);
  const withTx = blocks.filter((b) => b.transactions.length > 0).length;
  const donutData = [
    { name: "With Transactions", value: withTx },
    { name: "Empty Blocks", value: blocks.length - withTx },
  ].filter((d) => d.value > 0);

  const recentBlocks = [...blocks].reverse().slice(0, 10);
  const recentPending = pending.slice(0, 5);

  if (healthErr && !health) {
    return (
      <PageWrapper title="Overview" subtitle="Blockchain health at a glance">
        <ErrorCard message={healthErr?.message} onRetry={retryHealth} />
      </PageWrapper>
    );
  }

  return (
    <PageWrapper title="Overview" subtitle="Blockchain health at a glance" updatedAt={updatedAt}>

      {/* ── KPI Row ─────────────────────────────────────────────────────── */}
      <div style={{ display: "grid", gridTemplateColumns: "repeat(5, 1fr)", gap: 14, marginBottom: 28 }} className="grid-3-col">
        {healthLoading ? (
          Array.from({ length: 5 }).map((_, i) => <SkeletonCard key={i} rows={1} />)
        ) : (
          <>
            <StatWidget
              label="System Health"
              value={health?.status === "ok" ? "Online" : "Offline"}
              icon={<Activity size={14} />}
            />
            <StatWidget
              label="Total Blocks"
              value={summaryLoading ? "—" : (summary?.totalBlocks ?? 0)}
              icon={<Blocks size={14} />}
            />
            <StatWidget
              label="Latest Hash"
              value={summary ? String(summary.latestHash).slice(0, 10) : "—"}
              mono
              icon={<Clock size={14} />}
            />
            <StatWidget
              label="Pending TXs"
              value={pendingCount}
              icon={<Zap size={14} />}
            />
            <StatWidget
              label="Chain Integrity"
              value={verifyLoading ? "Checking…" : verify?.valid ? "Valid" : "Compromised"}
              icon={<ShieldCheck size={14} />}
            />
          </>
        )}
      </div>

      {/* ── Charts ──────────────────────────────────────────────────────── */}
      {blocksLoading ? (
        <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 16, marginBottom: 28 }} className="grid-3-col">
          {Array.from({ length: 3 }).map((_, i) => <SkeletonCard key={i} rows={4} />)}
        </div>
      ) : blocksErr ? (
        <div style={{ marginBottom: 28 }}><ErrorCard message={blocksErr?.message} /></div>
      ) : (
        <div style={{ display: "grid", gridTemplateColumns: "repeat(3, 1fr)", gap: 16, marginBottom: 28 }} className="grid-3-col">
          <Card>
            <p style={{ fontSize: 12, fontWeight: 600, letterSpacing: "0.06em", textTransform: "uppercase", color: "#6b6560", marginBottom: 16, fontFamily: "var(--font-body)" }}>
              Transactions per Block
            </p>
            <BarChart data={txPerBlock} xKey="block" yKey="txs" color="#c96442" height={200} />
          </Card>
          <Card>
            <p style={{ fontSize: 12, fontWeight: 600, letterSpacing: "0.06em", textTransform: "uppercase", color: "#6b6560", marginBottom: 16, fontFamily: "var(--font-body)" }}>
              Cumulative Volume
            </p>
            <AreaChart data={cumulativeVolume} xKey="block" yKey="volume" color="#c9a842" height={200} formatter={(v) => formatAmount(Number(v))} />
          </Card>
          <Card>
            <p style={{ fontSize: 12, fontWeight: 600, letterSpacing: "0.06em", textTransform: "uppercase", color: "#6b6560", marginBottom: 16, fontFamily: "var(--font-body)" }}>
              Block Composition
            </p>
            <DonutChart data={donutData} height={200} />
          </Card>
        </div>
      )}

      {/* ── Live Feeds ──────────────────────────────────────────────────── */}
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 16 }} className="grid-2-col">

        {/* Recent Blocks */}
        <Card padding="none">
          <div style={{ padding: "14px 20px", borderBottom: "1px solid #1e1e1e", display: "flex", alignItems: "center", justifyContent: "space-between" }}>
            <span style={{ fontSize: 13, fontWeight: 600, color: "#f5f0e8", fontFamily: "var(--font-body)" }}>Recent Blocks</span>
            <Link href="/blocks" style={{ fontSize: 12, color: "#c96442", fontFamily: "var(--font-body)", textDecoration: "none" }}>View all →</Link>
          </div>
          <div style={{ overflowX: "auto" }}>
            <table>
              <thead>
                <tr>
                  {["Index", "Hash", "TXs", "Time"].map(h => (
                    <th key={h}>{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {recentBlocks.length === 0 ? (
                  <tr><td colSpan={4} style={{ textAlign: "center", padding: "32px 20px", color: "#6b6560" }}>No blocks yet</td></tr>
                ) : recentBlocks.map((b) => (
                  <tr key={b.index}>
                    <td>
                      <Link href={`/blocks/${b.index}`} style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#c96442", textDecoration: "none", fontWeight: 500 }}>
                        #{b.index}
                      </Link>
                    </td>
                    <td><HashDisplay hash={b.hash} /></td>
                    <td><Badge variant={b.transactions.length > 0 ? "info" : "default"}>{b.transactions.length}</Badge></td>
                    <td style={{ fontSize: 12, color: "#6b6560" }}>{timeAgo(b.timestamp)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </Card>

        {/* Pending Transactions */}
        <Card padding="none">
          <div style={{ padding: "14px 20px", borderBottom: "1px solid #1e1e1e", display: "flex", alignItems: "center", justifyContent: "space-between" }}>
            <span style={{ fontSize: 13, fontWeight: 600, color: "#f5f0e8", fontFamily: "var(--font-body)" }}>Pending Transactions</span>
            <Link href="/mine" style={{ fontSize: 12, color: "#c96442", fontFamily: "var(--font-body)", textDecoration: "none" }}>Mine →</Link>
          </div>
          <div style={{ overflowX: "auto" }}>
            <table>
              <thead>
                <tr>
                  {["ID", "Sender", "Receiver", "Amount"].map(h => (
                    <th key={h}>{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {recentPending.length === 0 ? (
                  <tr><td colSpan={4} style={{ textAlign: "center", padding: "32px 20px", color: "#6b6560" }}>Mempool is clear</td></tr>
                ) : recentPending.map((tx) => (
                  <tr key={tx.id}>
                    <td style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#c96442" }}>#{tx.id}</td>
                    <td>{tx.sender}</td>
                    <td>{tx.receiver}</td>
                    <td style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#c9a842" }}>{formatAmount(tx.amount)}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </Card>

      </div>
    </PageWrapper>
  );
}
