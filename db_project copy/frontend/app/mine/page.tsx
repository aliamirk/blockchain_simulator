"use client";
import { useState, useEffect } from "react";
import { Pickaxe, Zap } from "lucide-react";
import { PageWrapper } from "@/components/layout/PageWrapper";
import { Card } from "@/components/ui/Card";
import { Button } from "@/components/ui/Button";
import { Badge } from "@/components/ui/Badge";
import { EmptyState } from "@/components/ui/EmptyState";
import { AreaChart } from "@/components/charts/AreaChart";
import { BarChart } from "@/components/charts/BarChart";
import { usePending } from "@/hooks/useTransactions";
import { useMine } from "@/hooks/useMine";
import { useToast } from "@/components/ui/Toast";
import { formatAmount, bucketAmounts } from "@/lib/utils";
import { mutate } from "swr";

interface PendingSnapshot {
  time: string;
  count: number;
  [key: string]: string | number;
}

export default function MinePage() {
  const { pending, count, isLoading, mutate: mutatePending } = usePending();
  const { mine, mining } = useMine();
  const { toast } = useToast();
  const [mineLog, setMineLog] = useState<{ blockIndex: number; txCount: number; time: string }[]>([]);
  const [pendingHistory, setPendingHistory] = useState<PendingSnapshot[]>(() => {
    const now = new Date().toLocaleTimeString();
    return [{ time: now, count: 0 }];
  });

  useEffect(() => {
    const now = new Date().toLocaleTimeString();
    setPendingHistory(prev => [...prev, { time: now, count }].slice(-20));
  }, [count]);

  const handleMine = async () => {
    const result = await mine();
    if (result) {
      toast("success", `Block #${result.block.index} mined with ${result.block.transactions.length} transactions`);
      setMineLog(prev => [{ blockIndex: result.block.index, txCount: result.block.transactions.length, time: new Date().toLocaleTimeString() }, ...prev]);
      mutatePending();
      mutate("/api/blockchain/blocks");
      mutate("/api/blockchain/summary");
    } else {
      toast("error", "Mining failed — check the API server");
    }
  };

  const amountBuckets = bucketAmounts(pending.map(tx => tx.amount)).filter(b => b.count > 0);
  const thStyle: React.CSSProperties = { padding: "10px 20px", textAlign: "left", fontSize: 11, fontWeight: 600, letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560", borderBottom: "1px solid #222222", fontFamily: "var(--font-body)" };
  const tdStyle: React.CSSProperties = { padding: "13px 20px", borderBottom: "1px solid #1a1a1a", fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)" };

  return (
    <PageWrapper title="Mining Control" subtitle="Manage the mempool and mine new blocks" updatedAt={Date.now()}>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 340px", gap: 24, alignItems: "start" }} className="grid-sidebar">
        {/* Left: Mempool + charts */}
        <div style={{ display: "flex", flexDirection: "column", gap: 20 }}>
          <Card padding="none">
            <div style={{ padding: "16px 24px", borderBottom: "1px solid #222222", display: "flex", alignItems: "center", justifyContent: "space-between" }}>
              <p style={{ fontSize: 14, fontWeight: 500, color: "#f5f0e8", fontFamily: "var(--font-body)", margin: 0 }}>Mempool</p>
              <Badge variant={count > 0 ? "warning" : "default"}>{count} pending</Badge>
            </div>
            {isLoading ? (
              <div style={{ padding: "32px 24px", textAlign: "center", color: "#6b6560", fontFamily: "var(--font-body)", fontSize: 14 }}>Loading…</div>
            ) : pending.length === 0 ? (
              <EmptyState title="Mempool is clear" description="Add transactions from the Transactions page" icon={<Zap size={36} strokeWidth={1.5} />} />
            ) : (
              <div style={{ overflowX: "auto" }}>
                <table style={{ width: "100%", borderCollapse: "collapse" }}>
                  <thead><tr>{["TX ID", "Sender", "Receiver", "Amount"].map(h => <th key={h} style={thStyle}>{h}</th>)}</tr></thead>
                  <tbody>
                    {pending.map(tx => (
                      <tr key={tx.id}
                        onMouseEnter={e => (e.currentTarget as HTMLTableRowElement).style.background = "#1a1a1a"}
                        onMouseLeave={e => (e.currentTarget as HTMLTableRowElement).style.background = "transparent"}
                      >
                        <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13, color: "#c96442" }}>#{tx.id}</td>
                        <td style={tdStyle}>{tx.sender}</td>
                        <td style={tdStyle}>{tx.receiver}</td>
                        <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13, color: "#c9a842" }}>{formatAmount(tx.amount)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </Card>

          <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 20 }} className="grid-2-col">
            <Card>
              <p style={{ fontSize: 13, fontWeight: 500, color: "#c9c3b8", marginBottom: 12, fontFamily: "var(--font-body)" }}>Pending Count Over Time</p>
              <AreaChart data={pendingHistory} xKey="time" yKey="count" color="#c9a842" height={180} />
            </Card>
            <Card>
              <p style={{ fontSize: 13, fontWeight: 500, color: "#c9c3b8", marginBottom: 12, fontFamily: "var(--font-body)" }}>Amount Distribution</p>
              <BarChart data={amountBuckets} xKey="range" yKey="count" color="#4278c9" height={180} />
              {amountBuckets.length === 0 && (
                <p style={{ textAlign: "center", color: "#6b6560", fontSize: 13, fontFamily: "var(--font-body)", marginTop: 8 }}>
                  No pending transactions
                </p>
              )}
            </Card>
          </div>
        </div>

        {/* Right: Mine panel */}
        <div style={{ display: "flex", flexDirection: "column", gap: 20 }}>
          <Card style={{ textAlign: "center" }}>
            <div style={{
              width: 80, height: 80, borderRadius: "50%", margin: "0 auto 20px",
              background: "rgba(201,100,66,0.1)", border: "1px solid rgba(201,100,66,0.3)",
              display: "flex", alignItems: "center", justifyContent: "center",
            }}>
              <Pickaxe size={36} color="#c96442" strokeWidth={1.5} />
            </div>
            <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 22, color: "#f5f0e8", marginBottom: 8 }}>Mine Block</h3>
            <p style={{ fontSize: 14, color: "#6b6560", fontFamily: "var(--font-body)", marginBottom: 24 }}>
              {count > 0 ? `${count} transaction${count !== 1 ? "s" : ""} ready` : "No pending transactions"}
            </p>
            <Button size="lg" loading={mining} disabled={count === 0} onClick={handleMine} style={{ width: "100%" }}>
              <Pickaxe size={16} />
              {mining ? "Mining…" : "Mine Block"}
            </Button>
          </Card>

          <Card>
            <p style={{ fontSize: 14, fontWeight: 500, color: "#f5f0e8", fontFamily: "var(--font-body)", marginBottom: 16 }}>Session Log</p>
            {mineLog.length === 0 ? (
              <p style={{ fontSize: 13, color: "#6b6560", fontFamily: "var(--font-body)" }}>No blocks mined this session</p>
            ) : (
              <div style={{ display: "flex", flexDirection: "column", gap: 0 }}>
                {mineLog.map((entry, i) => (
                  <div key={i} style={{
                    display: "flex", alignItems: "center", justifyContent: "space-between",
                    padding: "10px 0", borderBottom: i < mineLog.length - 1 ? "1px solid #1a1a1a" : "none",
                  }}>
                    <span style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#c96442" }}>Block #{entry.blockIndex}</span>
                    <span style={{ fontSize: 12, color: "#6b6560", fontFamily: "var(--font-body)" }}>{entry.txCount} txs · {entry.time}</span>
                  </div>
                ))}
              </div>
            )}
          </Card>
        </div>
      </div>
    </PageWrapper>
  );
}
