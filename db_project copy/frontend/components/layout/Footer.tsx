"use client";
import { useHealth } from "@/hooks/useHealth";
import { useSummary } from "@/hooks/useChain";

export function Footer() {
  const { health } = useHealth();
  const { summary } = useSummary();

  return (
    <footer style={{
      borderTop: "1px solid #222222",
      background: "#0a0a0a",
      padding: "16px 48px",
    }}>
      <div style={{
        maxWidth: 1280, margin: "0 auto",
        display: "flex", alignItems: "center", justifyContent: "space-between",
        flexWrap: "wrap", gap: 8,
      }}>
        <div style={{ display: "flex", alignItems: "center", gap: 16, fontSize: 12, fontFamily: "var(--font-body)" }}>
          <span style={{ color: "#6b6560" }}>
            Health:{" "}
            <span style={{ color: health?.status === "ok" ? "#4a9e6b" : "#c94242" }}>
              {health?.status ?? "—"}
            </span>
          </span>
          <span style={{ color: "#333" }}>·</span>
          <span style={{ color: "#6b6560" }}>Chain: <span style={{ color: "#c9c3b8" }}>{summary?.chainName ?? "—"}</span></span>
          <span style={{ color: "#333" }}>·</span>
          <span style={{ color: "#6b6560" }}>Blocks: <span style={{ color: "#c9c3b8" }}>{summary?.totalBlocks ?? "—"}</span></span>
        </div>
        <span style={{ fontSize: 12, color: "#6b6560", fontFamily: "var(--font-body)" }}>
          © {new Date().getFullYear()} Blockchain Dashboard
        </span>
      </div>
    </footer>
  );
}
