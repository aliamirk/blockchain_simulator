"use client";
import { useState } from "react";
import { ShieldCheck, ShieldX, Plus, Save, Download } from "lucide-react";
import { PageWrapper } from "@/components/layout/PageWrapper";
import { Card } from "@/components/ui/Card";
import { Button } from "@/components/ui/Button";
import { Badge } from "@/components/ui/Badge";
import { ErrorCard } from "@/components/ui/ErrorCard";
import { SkeletonTable } from "@/components/ui/Skeleton";
import { useChains, useSummary, useVerify } from "@/hooks/useChain";
import { createChain, loadChain, saveChain, fetchVerify } from "@/lib/api";
import { useToast } from "@/components/ui/Toast";
import { mutate } from "swr";

const inputStyle: React.CSSProperties = {
  background: "#1a1a1a", border: "1px solid #2a2a2a", borderRadius: 8,
  color: "#f5f0e8", fontFamily: "var(--font-body)", fontSize: 14,
  padding: "9px 14px", outline: "none", width: "100%",
};
const labelStyle: React.CSSProperties = {
  display: "block", fontSize: 11, fontWeight: 600, letterSpacing: "0.08em",
  textTransform: "uppercase", color: "#6b6560", marginBottom: 6, fontFamily: "var(--font-body)",
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

export default function ChainsPage() {
  const { chains, error, isLoading, mutate: mutateChains } = useChains();
  const { summary, mutate: mutateSummary } = useSummary();
  const { mutate: mutateVerify } = useVerify();
  const { toast } = useToast();

  const [createForm, setCreateForm] = useState({ name: "", difficulty: "3" });
  const [saveForm, setSaveForm] = useState("");
  const [creating, setCreating] = useState(false);
  const [saving, setSaving] = useState(false);
  const [loadingChain, setLoadingChain] = useState<string | null>(null);
  const [verifying, setVerifying] = useState(false);
  const [verifyResult, setVerifyResult] = useState<{ valid: boolean; message: string } | null>(null);
  const [createError, setCreateError] = useState<string | null>(null);

  const handleCreate = async (e: React.FormEvent) => {
    e.preventDefault();
    setCreateError(null);
    const diff = parseInt(createForm.difficulty, 10);
    if (!createForm.name.trim() || isNaN(diff) || diff < 1 || diff > 5) {
      setCreateError("Name required, difficulty must be 1–5");
      return;
    }
    setCreating(true);
    try {
      await createChain(createForm.name.trim(), diff);
      toast("success", `Chain "${createForm.name}" created`);
      setCreateForm({ name: "", difficulty: "3" });
      mutateChains(); mutateSummary(); mutate("/api/blockchain/summary");
    } catch (err) {
      toast("error", err instanceof Error ? err.message : "Create failed");
    } finally { setCreating(false); }
  };

  const handleLoad = async (name: string) => {
    setLoadingChain(name);
    try {
      await loadChain(name);
      toast("success", `Chain "${name}" loaded`);
      mutateChains(); mutateSummary();
      mutate("/api/blockchain/summary"); mutate("/api/blockchain/blocks");
      mutate("/api/accounts"); mutate("/api/transactions/pending");
    } catch (err) {
      toast("error", err instanceof Error ? err.message : "Load failed");
    } finally { setLoadingChain(null); }
  };

  const handleSave = async (e: React.FormEvent) => {
    e.preventDefault();
    setSaving(true);
    try {
      await saveChain(saveForm.trim() || undefined);
      toast("success", "Chain saved to database");
      mutateChains();
    } catch (err) {
      toast("error", err instanceof Error ? err.message : "Save failed");
    } finally { setSaving(false); }
  };

  const handleVerify = async () => {
    setVerifying(true);
    try {
      const r = await fetchVerify();
      setVerifyResult(r); mutateVerify(r);
    } catch (err) {
      setVerifyResult({ valid: false, message: err instanceof Error ? err.message : "Verification failed" });
    } finally { setVerifying(false); }
  };

  return (
    <PageWrapper title="Chain Management" subtitle="Create, load, save, and verify blockchain instances" updatedAt={Date.now()}>
      <div style={{ display: "grid", gridTemplateColumns: "1fr 340px", gap: 24, alignItems: "start" }} className="grid-sidebar">
        {/* Left: Saved chains */}
        <Card padding="none">
          <div style={{ padding: "16px 24px", borderBottom: "1px solid #222222" }}>
            <p style={{ fontSize: 14, fontWeight: 500, color: "#f5f0e8", fontFamily: "var(--font-body)", margin: 0 }}>Saved Chains</p>
          </div>
          {isLoading ? (
            <div style={{ padding: 24 }}><SkeletonTable rows={4} /></div>
          ) : error ? (
            <div style={{ padding: 24 }}><ErrorCard message={error?.message} onRetry={mutateChains} /></div>
          ) : chains.length === 0 ? (
            <div style={{ padding: "48px 24px", textAlign: "center", color: "#6b6560", fontFamily: "var(--font-body)", fontSize: 14 }}>
              No chains saved yet
            </div>
          ) : (
            <div style={{ overflowX: "auto" }}>
              <table style={{ width: "100%", borderCollapse: "collapse" }}>
                <thead>
                  <tr>{["Name", "Difficulty", "Blocks", "TXs", "Created", ""].map((h, i) => <th key={i} style={thStyle}>{h}</th>)}</tr>
                </thead>
                <tbody>
                  {chains.map(chain => {
                    const isActive = summary?.chainName === chain.name;
                    return (
                      <tr key={chain.name}
                        onMouseEnter={e => (e.currentTarget as HTMLTableRowElement).style.background = "#1a1a1a"}
                        onMouseLeave={e => (e.currentTarget as HTMLTableRowElement).style.background = "transparent"}
                      >
                        <td style={{ ...tdStyle, fontWeight: 500, color: "#f5f0e8" }}>
                          <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
                            {chain.name}
                            {isActive && <Badge variant="info">Active</Badge>}
                          </div>
                        </td>
                        <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13 }}>{chain.difficulty}</td>
                        <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13 }}>{chain.totalBlocks}</td>
                        <td style={{ ...tdStyle, fontFamily: "var(--font-mono)", fontSize: 13 }}>{chain.totalTx}</td>
                        <td style={{ ...tdStyle, fontSize: 12, color: "#6b6560" }}>{chain.createdAt}</td>
                        <td style={tdStyle}>
                          <Button variant="secondary" size="sm" loading={loadingChain === chain.name} disabled={isActive} onClick={() => handleLoad(chain.name)}>
                            <Download size={12} /> Load
                          </Button>
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
          )}
        </Card>

        {/* Right: Action cards */}
        <div style={{ display: "flex", flexDirection: "column", gap: 20 }}>
          {/* Create */}
          <Card>
            <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 18, color: "#f5f0e8", marginBottom: 20, display: "flex", alignItems: "center", gap: 8 }}>
              <Plus size={16} color="#c96442" /> Create New Chain
            </h3>
            <form onSubmit={handleCreate} style={{ display: "flex", flexDirection: "column", gap: 14 }}>
              <div>
                <label style={labelStyle}>Chain Name</label>
                <input placeholder="e.g. mainchain" value={createForm.name}
                  onChange={e => setCreateForm(f => ({ ...f, name: e.target.value }))}
                  style={inputStyle}
                  onFocus={e => (e.target as HTMLInputElement).style.borderColor = "rgba(201,100,66,0.5)"}
                  onBlur={e => (e.target as HTMLInputElement).style.borderColor = "#2a2a2a"}
                />
              </div>
              <div>
                <label style={labelStyle}>Difficulty (1–5)</label>
                <input type="number" min={1} max={5} value={createForm.difficulty}
                  onChange={e => setCreateForm(f => ({ ...f, difficulty: e.target.value }))}
                  style={inputStyle}
                  onFocus={e => (e.target as HTMLInputElement).style.borderColor = "rgba(201,100,66,0.5)"}
                  onBlur={e => (e.target as HTMLInputElement).style.borderColor = "#2a2a2a"}
                />
              </div>
              {createError && <p style={{ fontSize: 12, color: "#c94242", fontFamily: "var(--font-body)", margin: 0 }}>{createError}</p>}
              <Button type="submit" loading={creating}>Create Chain</Button>
            </form>
          </Card>

          {/* Save */}
          <Card>
            <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 18, color: "#f5f0e8", marginBottom: 20, display: "flex", alignItems: "center", gap: 8 }}>
              <Save size={16} color="#c96442" /> Save Current Chain
            </h3>
            <form onSubmit={handleSave} style={{ display: "flex", flexDirection: "column", gap: 14 }}>
              <div>
                <label style={labelStyle}>Name Override (optional)</label>
                <input placeholder={summary?.chainName ?? "Current chain name"} value={saveForm}
                  onChange={e => setSaveForm(e.target.value)}
                  style={inputStyle}
                  onFocus={e => (e.target as HTMLInputElement).style.borderColor = "rgba(201,100,66,0.5)"}
                  onBlur={e => (e.target as HTMLInputElement).style.borderColor = "#2a2a2a"}
                />
              </div>
              <Button type="submit" loading={saving} variant="secondary">Save to Database</Button>
            </form>
          </Card>

          {/* Verify */}
          <Card>
            <h3 style={{ fontFamily: "var(--font-heading)", fontSize: 18, color: "#f5f0e8", marginBottom: 20, display: "flex", alignItems: "center", gap: 8 }}>
              <ShieldCheck size={16} color="#c96442" /> Verify Integrity
            </h3>
            {verifyResult && (
              <div style={{
                display: "flex", alignItems: "center", gap: 10,
                padding: "12px 16px", borderRadius: 8, marginBottom: 16, fontSize: 14,
                background: verifyResult.valid ? "rgba(74,158,107,0.1)" : "rgba(201,66,66,0.1)",
                color: verifyResult.valid ? "#4a9e6b" : "#c94242",
                border: `1px solid ${verifyResult.valid ? "#4a9e6b" : "#c94242"}`,
                fontFamily: "var(--font-body)",
              }}>
                {verifyResult.valid ? <ShieldCheck size={16} /> : <ShieldX size={16} />}
                {verifyResult.message}
              </div>
            )}
            <Button variant="secondary" loading={verifying} onClick={handleVerify} style={{ width: "100%" }}>
              Run Integrity Check
            </Button>
          </Card>
        </div>
      </div>
    </PageWrapper>
  );
}
