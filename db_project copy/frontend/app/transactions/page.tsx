"use client";
import { useState, useMemo } from "react";
import Link from "next/link";
import { Plus, X } from "lucide-react";
import { PageWrapper } from "@/components/layout/PageWrapper";
import { Card } from "@/components/ui/Card";
import { Button } from "@/components/ui/Button";
import { ErrorCard } from "@/components/ui/ErrorCard";
import { SkeletonTable } from "@/components/ui/Skeleton";
import { BarChart } from "@/components/charts/BarChart";
import { useBlocks } from "@/hooks/useBlocks";
import { usePending } from "@/hooks/useTransactions";
import { addTransaction } from "@/lib/api";
import { useToast } from "@/components/ui/Toast";
import { formatAmount } from "@/lib/utils";
import type { Transaction } from "@/lib/types";
import { mutate } from "swr";

interface FlatTx extends Transaction { blockIndex: number; }

const inputStyle: React.CSSProperties = {
  background: "#1a1a1a", border: "1px solid #2a2a2a", borderRadius: 8,
  color: "#f5f0e8", fontFamily: "var(--font-body)", fontSize: 14,
  padding: "9px 14px", outline: "none", width: "100%",
  transition: "border-color 200ms",
};
const labelStyle: React.CSSProperties = {
  display: "block", fontSize: 11, fontWeight: 600, letterSpacing: "0.08em",
  textTransform: "uppercase", color: "#6b6560", marginBottom: 6,
  fontFamily: "var(--font-body)",
};
const thStyle: React.CSSProperties = {
  padding: "10px 20px", textAlign: "left", fontSize: 11, fontWeight: 600,
  letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560",
  borderBottom: "1px solid #222222", fontFamily: "var(--font-body)", whiteSpace: "nowrap",
};
const tdStyle: React.CSSProperties = {
  padding: "13px 20px", borderBottom: "1px solid #1a1a1a",
  fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)",
};

export default function TransactionsPage() {
  const { blocks, error, isLoading, mutate: mutateBlocks } = useBlocks();
  const { mutate: mutatePending } = usePending();
  const { toast } = useToast();
  const [showForm, setShowForm] = useState(false);
  const [submitting, setSubmitting] = useState(false);
  const [formError, setFormError] = useState<string | null>(null);
  const [form, setForm] = useState({ sender: "", receiver: "", amount: "", metadata: "" });

  const allTxs: FlatTx[] = useMemo(
    () => blocks.flatMap((b) => b.transactions.map((tx) => ({ ...tx, blockIndex: b.index }))),
    [blocks]
  );

  const senderTotals = useMemo(() => {
    const map: Record<string, number> = {};
    allTxs.forEach((tx) => { map[tx.sender] = (map[tx.sender] ?? 0) + tx.amount; });
    return Object.entries(map).sort((a, b) => b[1] - a[1]).slice(0, 10)
      .map(([name, total]) => ({ name, total: parseFloat(total.toFixed(2)) }));
  }, [allTxs]);

  const receiverTotals = useMemo(() => {
    const map: Record<string, number> = {};
    allTxs.forEach((tx) => { map[tx.receiver] = (map[tx.receiver] ?? 0) + tx.amount; });
    return Object.entries(map).sort((a, b) => b[1] - a[1]).slice(0, 10)
      .map(([name, total]) => ({ name, total: parseFloat(total.toFixed(2)) }));
  }, [allTxs]);

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();
    setFormError(null);
    const amount = parseFloat(form.amount);
    if (!form.sender || !form.receiver || isNaN(amount) || amount <= 0) {
      setFormError("All fields are required and amount must be positive.");
      return;
    }
    setSubmitting(true);
    try {
      await addTransaction(form.sender, form.receiver, amount, form.metadata || undefined);
      toast("success", `Transaction added: ${form.sender} → ${form.receiver} ${formatAmount(amount)}`);
      setForm({ sender: "", receiver: "", amount: "", metadata: "" });
      setShowForm(false);
      mutatePending();
      mutate("/api/transactions/pending");
    } catch (err) {
      setFormError(err instanceof Error ? err.message : "Failed to add transaction");
    } finally {
      setSubmitting(false);
    }
  };

  const fields = [
    { label: "Sender", key: "sender", type: "text", placeholder: "e.g. Alice" },
    { label: "Receiver", key: "receiver", type: "text", placeholder: "e.g. Bob" },
    { label: "Amount ($)", key: "amount", type: "number", placeholder: "0.00" },
    { label: "Metadata (optional)", key: "metadata", type: "text", placeholder: "Note…" },
  ];

  return (
    <PageWrapper title="Transactions" subtitle="All confirmed transactions and the pending mempool" updatedAt={Date.now()}>
      {/* Header */}
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 24 }}>
        <p style={{ fontSize: 14, color: "#6b6560", fontFamily: "var(--font-body)", margin: 0 }}>
          {allTxs.length} confirmed transactions
        </p>
        <Button onClick={() => setShowForm(true)}>
          <Plus size={14} /> New Transaction
        </Button>
      </div>

      {/* Modal */}
      {showForm && (
        <div style={{ position: "fixed", inset: 0, zIndex: 50, display: "flex", alignItems: "center", justifyContent: "center" }}>
          <div style={{ position: "absolute", inset: 0, background: "rgba(0,0,0,0.75)" }} onClick={() => setShowForm(false)} />
          <div style={{
            position: "relative", width: "100%", maxWidth: 460,
            background: "#111111", border: "1px solid #2a2a2a",
            borderRadius: 16, padding: 32, margin: "0 16px",
            animation: "slide-in-right 250ms ease-out",
          }}>
            <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 24 }}>
              <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 22, color: "#f5f0e8", margin: 0 }}>New Transaction</h3>
              <button onClick={() => setShowForm(false)} style={{ background: "none", border: "none", cursor: "pointer", color: "#6b6560", padding: 4 }}>
                <X size={18} />
              </button>
            </div>
            <form onSubmit={handleSubmit} style={{ display: "flex", flexDirection: "column", gap: 16 }}>
              {fields.map(({ label, key, type, placeholder }) => (
                <div key={key}>
                  <label style={labelStyle}>{label}</label>
                  <input
                    type={type}
                    placeholder={placeholder}
                    value={form[key as keyof typeof form]}
                    onChange={(e) => setForm((f) => ({ ...f, [key]: e.target.value }))}
                    step={type === "number" ? "0.01" : undefined}
                    min={type === "number" ? "0.01" : undefined}
                    style={inputStyle}
                    onFocus={e => (e.target as HTMLInputElement).style.borderColor = "rgba(201,100,66,0.5)"}
                    onBlur={e => (e.target as HTMLInputElement).style.borderColor = "#2a2a2a"}
                  />
                </div>
              ))}
              {formError && (
                <p style={{ fontSize: 13, color: "#c94242", fontFamily: "var(--font-body)", margin: 0 }}>{formError}</p>
              )}
              <Button type="submit" loading={submitting} style={{ marginTop: 8 }}>
                Submit Transaction
              </Button>
            </form>
          </div>
        </div>
      )}

      {/* Charts */}
      {!isLoading && allTxs.length > 0 && (
        <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 20, marginBottom: 24 }}>
          <Card>
            <p style={{ fontSize: 13, fontWeight: 500, color: "#c9c3b8", marginBottom: 12, fontFamily: "var(--font-body)" }}>Top Senders by Volume</p>
            <BarChart data={senderTotals} xKey="name" yKey="total" layout="horizontal" color="#c96442" formatter={(v) => formatAmount(Number(v))} height={280} />
          </Card>
          <Card>
            <p style={{ fontSize: 13, fontWeight: 500, color: "#c9c3b8", marginBottom: 12, fontFamily: "var(--font-body)" }}>Top Receivers by Volume</p>
            <BarChart data={receiverTotals} xKey="name" yKey="total" layout="horizontal" color="#4a9e6b" formatter={(v) => formatAmount(Number(v))} height={280} />
          </Card>
        </div>
      )}

      {/* Table */}
      <Card padding="none">
        {isLoading ? (
          <div style={{ padding: 24 }}><SkeletonTable rows={8} /></div>
        ) : error ? (
          <div style={{ padding: 24 }}><ErrorCard message={error?.message} onRetry={mutateBlocks} /></div>
        ) : allTxs.length === 0 ? (
          <div style={{ padding: "64px 24px", textAlign: "center", color: "#6b6560", fontFamily: "var(--font-body)", fontSize: 14 }}>
            No confirmed transactions yet. Add one and mine a block.
          </div>
        ) : (
          <div style={{ overflowX: "auto" }}>
            <table style={{ width: "100%", borderCollapse: "collapse" }}>
              <thead>
                <tr>{["TX ID", "Sender", "Receiver", "Amount", "Metadata", "Block"].map(h => <th key={h} style={thStyle}>{h}</th>)}</tr>
              </thead>
              <tbody>
                {allTxs.map((tx) => (
                  <tr key={`${tx.blockIndex}-${tx.id}`}
                    style={{ cursor: "pointer", transition: "background 150ms" }}
                    onMouseEnter={e => (e.currentTarget as HTMLTableRowElement).style.background = "#1a1a1a"}
                    onMouseLeave={e => (e.currentTarget as HTMLTableRowElement).style.background = "transparent"}
                    onClick={() => window.location.href = `/transactions/${tx.id}`}
                  >
                    <td style={tdStyle}>
                      <Link href={`/transactions/${tx.id}`} onClick={e => e.stopPropagation()}
                        style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#c96442", textDecoration: "none", fontWeight: 500 }}>
                        #{tx.id}
                      </Link>
                    </td>
                    <td style={tdStyle}>{tx.sender}</td>
                    <td style={tdStyle}>{tx.receiver}</td>
                    <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13, color: "#c9a842" }}>{formatAmount(tx.amount)}</td>
                    <td style={{ ...tdStyle, color: "#6b6560" }}>{tx.metadata || "—"}</td>
                    <td style={tdStyle}>
                      <Link href={`/blocks/${tx.blockIndex}`} onClick={e => e.stopPropagation()}
                        style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#4278c9", textDecoration: "none" }}>
                        #{tx.blockIndex}
                      </Link>
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
