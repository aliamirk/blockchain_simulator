"use client";
import { useState } from "react";
import Link from "next/link";
import { ShieldCheck, ShieldX } from "lucide-react";
import { PageWrapper } from "@/components/layout/PageWrapper";
import { Card } from "@/components/ui/Card";
import { Button } from "@/components/ui/Button";
import { Badge } from "@/components/ui/Badge";
import { HashDisplay } from "@/components/ui/HashDisplay";
import { ErrorCard } from "@/components/ui/ErrorCard";
import { SkeletonTable } from "@/components/ui/Skeleton";
import { BarChart } from "@/components/charts/BarChart";
import { useBlocks } from "@/hooks/useBlocks";
import { useVerify } from "@/hooks/useChain";
import { fetchVerify } from "@/lib/api";
import { timeAgo } from "@/lib/utils";

const PAGE_SIZE = 20;

export default function BlocksPage() {
  const { blocks, error, isLoading, mutate } = useBlocks();
  const { mutate: mutateVerify } = useVerify();
  const [page, setPage] = useState(0);
  const [verifying, setVerifying] = useState(false);
  const [verifyResult, setVerifyResult] = useState<{ valid: boolean; message: string } | null>(null);

  const totalPages = Math.ceil(blocks.length / PAGE_SIZE);
  const pageBlocks = blocks.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE);
  const sparkData = pageBlocks.map((b) => ({ block: `#${b.index}`, txs: b.transactions.length }));

  const handleVerify = async () => {
    setVerifying(true);
    try {
      const r = await fetchVerify();
      setVerifyResult(r);
      mutateVerify(r);
    } catch (e) {
      setVerifyResult({ valid: false, message: e instanceof Error ? e.message : "Verification failed" });
    } finally {
      setVerifying(false);
    }
  };

  const thStyle: React.CSSProperties = {
    padding: "10px 20px", textAlign: "left", fontSize: 11, fontWeight: 600,
    letterSpacing: "0.08em", textTransform: "uppercase", color: "#6b6560",
    borderBottom: "1px solid #222222", fontFamily: "var(--font-body)",
    whiteSpace: "nowrap",
  };
  const tdStyle: React.CSSProperties = {
    padding: "13px 20px", borderBottom: "1px solid #1a1a1a",
    fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)",
  };

  return (
    <PageWrapper title="Block Explorer" subtitle="Browse every block on the chain" updatedAt={Date.now()}>
      {/* Verify banner */}
      {verifyResult && (
        <div style={{
          display: "flex", alignItems: "center", gap: 12,
          borderRadius: 10, padding: "14px 20px", marginBottom: 24,
          background: verifyResult.valid ? "rgba(74,158,107,0.08)" : "rgba(201,66,66,0.08)",
          border: `1px solid ${verifyResult.valid ? "#4a9e6b" : "#c94242"}`,
        }}>
          {verifyResult.valid
            ? <ShieldCheck size={18} color="#4a9e6b" />
            : <ShieldX size={18} color="#c94242" />}
          <span style={{ fontSize: 14, color: verifyResult.valid ? "#4a9e6b" : "#c94242", fontFamily: "var(--font-body)" }}>
            {verifyResult.message}
          </span>
        </div>
      )}

      {/* Actions row */}
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 20 }}>
        <p style={{ fontSize: 14, color: "#6b6560", fontFamily: "var(--font-body)", margin: 0 }}>
          {blocks.length} blocks total
        </p>
        <Button variant="secondary" size="sm" loading={verifying} onClick={handleVerify}>
          <ShieldCheck size={14} />
          Verify Chain Integrity
        </Button>
      </div>

      {/* Sparkline */}
      {!isLoading && blocks.length > 0 && (
        <Card style={{ marginBottom: 20 }}>
          <p style={{ fontSize: 13, fontWeight: 500, color: "#c9c3b8", marginBottom: 12, fontFamily: "var(--font-body)" }}>
            Transactions per Block (current page)
          </p>
          <BarChart data={sparkData} xKey="block" yKey="txs" height={140} color="#c96442" />
        </Card>
      )}

      {/* Table */}
      <Card padding="none">
        {isLoading ? (
          <div style={{ padding: 24 }}><SkeletonTable rows={8} /></div>
        ) : error ? (
          <div style={{ padding: 24 }}><ErrorCard message={error?.message} onRetry={mutate} /></div>
        ) : (
          <div style={{ overflowX: "auto" }}>
            <table style={{ width: "100%", borderCollapse: "collapse" }}>
              <thead>
                <tr>
                  {["Index", "Hash", "Prev Hash", "Timestamp", "Nonce", "TXs"].map(h => (
                    <th key={h} style={thStyle}>{h}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {pageBlocks.map((b) => (
                  <tr key={b.index}
                    style={{ cursor: "pointer", transition: "background 150ms" }}
                    onMouseEnter={e => (e.currentTarget as HTMLTableRowElement).style.background = "#1a1a1a"}
                    onMouseLeave={e => (e.currentTarget as HTMLTableRowElement).style.background = "transparent"}
                    onClick={() => window.location.href = `/blocks/${b.index}`}
                  >
                    <td style={tdStyle}>
                      <Link href={`/blocks/${b.index}`} onClick={e => e.stopPropagation()}
                        style={{ fontFamily: "var(--font-mono)", fontSize: 13, color: "#c96442", textDecoration: "none", fontWeight: 500 }}>
                        #{b.index}
                      </Link>
                    </td>
                    <td style={tdStyle}><HashDisplay hash={b.hash} /></td>
                    <td style={tdStyle}><HashDisplay hash={b.prevHash} /></td>
                    <td style={{ ...tdStyle, fontSize: 12, color: "#6b6560" }}>{timeAgo(b.timestamp)}</td>
                    <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13 }}>{b.nonce}</td>
                    <td style={tdStyle}><Badge variant={b.transactions.length > 0 ? "info" : "default"}>{b.transactions.length}</Badge></td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}

        {/* Pagination */}
        {totalPages > 1 && (
          <div style={{
            display: "flex", alignItems: "center", justifyContent: "space-between",
            padding: "16px 24px", borderTop: "1px solid #222222",
          }}>
            <Button variant="secondary" size="sm" disabled={page === 0} onClick={() => setPage(p => p - 1)}>
              ← Previous
            </Button>
            <span style={{ fontSize: 13, color: "#6b6560", fontFamily: "var(--font-body)" }}>
              Page {page + 1} of {totalPages}
            </span>
            <Button variant="secondary" size="sm" disabled={page >= totalPages - 1} onClick={() => setPage(p => p + 1)}>
              Next →
            </Button>
          </div>
        )}
      </Card>
    </PageWrapper>
  );
}
