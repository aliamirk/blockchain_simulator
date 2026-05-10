"use client";
import { use } from "react";
import Link from "next/link";
import { PageWrapper } from "@/components/layout/PageWrapper";
import { Card } from "@/components/ui/Card";
import { HashDisplay } from "@/components/ui/HashDisplay";
import { Button } from "@/components/ui/Button";
import { ErrorCard } from "@/components/ui/ErrorCard";
import { SkeletonCard } from "@/components/ui/Skeleton";
import { formatAmount } from "@/lib/utils";
import useSWR from "swr";
import { swrFetcher } from "@/lib/api";
import type { SearchTxResponse } from "@/lib/types";

interface Props { params: Promise<{ id: string }>; }

const rowStyle: React.CSSProperties = {
  display: "flex", alignItems: "center", justifyContent: "space-between",
  gap: 16, padding: "12px 0", borderBottom: "1px solid #1a1a1a",
};
const dtStyle: React.CSSProperties = {
  fontSize: 11, fontWeight: 600, letterSpacing: "0.08em", textTransform: "uppercase",
  color: "#6b6560", fontFamily: "var(--font-body)", flexShrink: 0,
};
const ddStyle: React.CSSProperties = {
  fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)", textAlign: "right",
};

export default function TransactionDetailPage({ params }: Props) {
  const { id } = use(params);
  const txId = parseInt(id, 10);
  const { data, error, isLoading } = useSWR<SearchTxResponse>(
    !isNaN(txId) ? `/api/search/tx/${txId}` : null, swrFetcher
  );

  if (isLoading) return (
    <PageWrapper title={`Transaction #${id}`} subtitle="Transaction details">
      <SkeletonCard rows={6} />
    </PageWrapper>
  );

  if (error || !data?.found || !data.transaction) return (
    <PageWrapper title={`Transaction #${id}`} subtitle="Transaction details">
      <ErrorCard message={error?.message ?? "Transaction not found"} />
    </PageWrapper>
  );

  const tx = data.transaction;

  return (
    <PageWrapper title={`Transaction #${tx.id}`} subtitle="Confirmed transaction details">
      <div style={{ marginBottom: 24 }}>
        <Link href="/transactions"><Button variant="secondary" size="sm">← All Transactions</Button></Link>
      </div>

      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 24 }}>
        {/* Details */}
        <Card>
          <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 18, color: "#f5f0e8", marginBottom: 20 }}>Transaction Details</h3>
          <dl style={{ display: "flex", flexDirection: "column" }}>
            {[
              { label: "TX ID",     value: <span style={{ fontFamily: "var(--font-mono)", color: "#c96442" }}>#{tx.id}</span> },
              { label: "Sender",    value: tx.sender },
              { label: "Receiver",  value: tx.receiver },
              { label: "Amount",    value: <span style={{ fontFamily: "var(--font-mono)", color: "#c9a842" }}>{formatAmount(tx.amount)}</span> },
              { label: "Metadata",  value: tx.metadata || "—" },
              { label: "Signature", value: <HashDisplay hash={tx.signature} /> },
              { label: "Block",     value: <Link href={`/blocks/${tx.blockIndex}`} style={{ fontFamily: "var(--font-mono)", color: "#4278c9", textDecoration: "none" }}>#{tx.blockIndex}</Link> },
              { label: "Position",  value: <span style={{ fontFamily: "var(--font-mono)" }}>{tx.positionInBlock}</span> },
            ].map(({ label, value }) => (
              <div key={label} style={rowStyle}>
                <dt style={dtStyle}>{label}</dt>
                <dd style={ddStyle}>{value}</dd>
              </div>
            ))}
          </dl>
        </Card>

        {/* Flow diagram */}
        <Card style={{ display: "flex", flexDirection: "column", alignItems: "center", justifyContent: "center", gap: 0 }}>
          <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 18, color: "#f5f0e8", marginBottom: 32, alignSelf: "flex-start" }}>Transfer Flow</h3>
          <div style={{ display: "flex", flexDirection: "column", alignItems: "center", gap: 0, width: "100%" }}>
            {/* From */}
            <div style={{
              width: "100%", padding: "16px 24px", borderRadius: 10, textAlign: "center",
              background: "#1a1a1a", border: "1px solid #2a2a2a",
            }}>
              <p style={{ fontSize: 11, fontWeight: 600, letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560", marginBottom: 6, fontFamily: "var(--font-body)" }}>From</p>
              <p style={{ fontSize: 20, fontWeight: 600, color: "#f5f0e8", fontFamily: "var(--font-body)" }}>{tx.sender}</p>
            </div>

            {/* Arrow + amount */}
            <div style={{ display: "flex", flexDirection: "column", alignItems: "center", padding: "12px 0" }}>
              <div style={{ width: 2, height: 20, background: "linear-gradient(to bottom, #c96442, #c96442)" }} />
              <div style={{
                padding: "6px 20px", borderRadius: 999, margin: "6px 0",
                background: "rgba(201,100,66,0.12)", border: "1px solid rgba(201,100,66,0.3)",
              }}>
                <span style={{ fontFamily: "var(--font-mono)", fontSize: 15, fontWeight: 600, color: "#c96442" }}>
                  {formatAmount(tx.amount)}
                </span>
              </div>
              <div style={{ width: 2, height: 20, background: "#c96442" }} />
              {/* Arrow head */}
              <div style={{ width: 0, height: 0, borderLeft: "6px solid transparent", borderRight: "6px solid transparent", borderTop: "8px solid #c96442" }} />
            </div>

            {/* To */}
            <div style={{
              width: "100%", padding: "16px 24px", borderRadius: 10, textAlign: "center",
              background: "#1a1a1a", border: "1px solid #2a2a2a",
            }}>
              <p style={{ fontSize: 11, fontWeight: 600, letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560", marginBottom: 6, fontFamily: "var(--font-body)" }}>To</p>
              <p style={{ fontSize: 20, fontWeight: 600, color: "#f5f0e8", fontFamily: "var(--font-body)" }}>{tx.receiver}</p>
            </div>
          </div>
        </Card>
      </div>
    </PageWrapper>
  );
}
