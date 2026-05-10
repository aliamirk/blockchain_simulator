"use client";
import { use } from "react";
import Link from "next/link";
import { ArrowLeft, ArrowRight } from "lucide-react";
import { PageWrapper } from "@/components/layout/PageWrapper";
import { Card } from "@/components/ui/Card";
import { HashDisplay } from "@/components/ui/HashDisplay";
import { Badge } from "@/components/ui/Badge";
import { Button } from "@/components/ui/Button";
import { ErrorCard } from "@/components/ui/ErrorCard";
import { SkeletonCard } from "@/components/ui/Skeleton";
import { useBlock } from "@/hooks/useBlocks";
import { useSummary } from "@/hooks/useChain";
import { formatAmount, timeAgo } from "@/lib/utils";

interface Props { params: Promise<{ index: string }>; }

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

export default function BlockDetailPage({ params }: Props) {
  const { index: indexStr } = use(params);
  const index = parseInt(indexStr, 10);
  const { block, error, isLoading } = useBlock(isNaN(index) ? null : index);
  const { summary } = useSummary();
  const totalBlocks = summary?.totalBlocks ?? 0;

  if (isLoading) return (
    <PageWrapper title={`Block #${index}`} subtitle="Block details">
      <SkeletonCard rows={6} />
    </PageWrapper>
  );

  if (error || !block) return (
    <PageWrapper title={`Block #${index}`} subtitle="Block details">
      <ErrorCard message={error?.message ?? "Block not found"} />
    </PageWrapper>
  );

  return (
    <PageWrapper title={`Block #${block.index}`} subtitle={`Mined at ${block.timestamp}`}>
      {/* Navigation */}
      <div style={{ display: "flex", alignItems: "center", gap: 12, marginBottom: 28 }}>
        <Link href="/blocks"><Button variant="secondary" size="sm">← All Blocks</Button></Link>
        <div style={{ display: "flex", gap: 8, marginLeft: "auto" }}>
          {index > 0 && (
            <Link href={`/blocks/${index - 1}`}>
              <Button variant="secondary" size="sm"><ArrowLeft size={13} /> Previous</Button>
            </Link>
          )}
          {index < totalBlocks - 1 && (
            <Link href={`/blocks/${index + 1}`}>
              <Button variant="secondary" size="sm">Next <ArrowRight size={13} /></Button>
            </Link>
          )}
        </div>
      </div>

      {/* Metadata + Volume */}
      <div style={{ display: "grid", gridTemplateColumns: "1fr 1fr", gap: 20, marginBottom: 24 }}>
        <Card>
          <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 18, color: "#f5f0e8", marginBottom: 16 }}>Block Details</h3>
          <dl style={{ display: "flex", flexDirection: "column" }}>
            {[
              { label: "Index",        value: <span style={{ fontFamily: "var(--font-mono)", color: "#c96442" }}>#{block.index}</span> },
              { label: "Hash",         value: <HashDisplay hash={block.hash} /> },
              { label: "Prev Hash",    value: <HashDisplay hash={block.prevHash} /> },
              { label: "Nonce",        value: <span style={{ fontFamily: "var(--font-mono)" }}>{block.nonce}</span> },
              { label: "Timestamp",    value: timeAgo(block.timestamp) },
              { label: "Transactions", value: <Badge variant="info">{block.transactions.length}</Badge> },
            ].map(({ label, value }) => (
              <div key={label} style={rowStyle}>
                <dt style={{ fontSize: 11, fontWeight: 600, letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560", fontFamily: "var(--font-body)", flexShrink: 0 }}>{label}</dt>
                <dd style={{ fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)", textAlign: "right" }}>{value}</dd>
              </div>
            ))}
          </dl>
        </Card>

        <Card>
          <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 18, color: "#f5f0e8", marginBottom: 16 }}>Volume Summary</h3>
          <div style={{ display: "flex", flexDirection: "column", gap: 0 }}>
            {[
              { label: "Total Volume", value: formatAmount(block.transactions.reduce((s, t) => s + t.amount, 0)), color: "#c9a842" },
              { label: "Avg TX Amount", value: block.transactions.length > 0 ? formatAmount(block.transactions.reduce((s, t) => s + t.amount, 0) / block.transactions.length) : "—", color: "#c9c3b8" },
              { label: "TX Count", value: String(block.transactions.length), color: "#4278c9" },
            ].map(({ label, value, color }) => (
              <div key={label} style={{ display: "flex", justifyContent: "space-between", alignItems: "center", padding: "14px 0", borderBottom: "1px solid #1a1a1a" }}>
                <span style={{ fontSize: 11, fontWeight: 600, letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560", fontFamily: "var(--font-body)" }}>{label}</span>
                <span style={{ fontFamily: "var(--font-mono)", fontSize: 15, fontWeight: 600, color }}>{value}</span>
              </div>
            ))}
          </div>
        </Card>
      </div>

      {/* Transactions */}
      <Card padding="none">
        <div style={{ padding: "16px 24px", borderBottom: "1px solid #222222" }}>
          <p style={{ fontSize: 14, fontWeight: 500, color: "#f5f0e8", fontFamily: "var(--font-body)", margin: 0 }}>
            Transactions ({block.transactions.length})
          </p>
        </div>
        {block.transactions.length === 0 ? (
          <div style={{ padding: "48px 24px", textAlign: "center", color: "#6b6560", fontFamily: "var(--font-body)", fontSize: 14 }}>
            No transactions in this block
          </div>
        ) : (
          <div style={{ overflowX: "auto" }}>
            <table style={{ width: "100%", borderCollapse: "collapse" }}>
              <thead>
                <tr>{["TX ID", "Sender", "Receiver", "Amount", "Metadata", "Signature"].map(h => <th key={h} style={thStyle}>{h}</th>)}</tr>
              </thead>
              <tbody>
                {block.transactions.map((tx) => (
                  <tr key={tx.id}
                    onMouseEnter={e => (e.currentTarget as HTMLTableRowElement).style.background = "#1a1a1a"}
                    onMouseLeave={e => (e.currentTarget as HTMLTableRowElement).style.background = "transparent"}
                  >
                    <td style={tdStyle}>
                      <Link href={`/transactions/${tx.id}`} style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#c96442", textDecoration: "none", fontWeight: 500 }}>
                        #{tx.id}
                      </Link>
                    </td>
                    <td style={tdStyle}>{tx.sender}</td>
                    <td style={tdStyle}>{tx.receiver}</td>
                    <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13, color: "#c9a842" }}>{formatAmount(tx.amount)}</td>
                    <td style={{ ...tdStyle, color: "#6b6560" }}>{tx.metadata || "—"}</td>
                    <td style={tdStyle}><HashDisplay hash={tx.signature} /></td>
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
