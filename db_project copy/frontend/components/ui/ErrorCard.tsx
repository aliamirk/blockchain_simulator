"use client";
import { AlertCircle, RefreshCw, Terminal } from "lucide-react";
import { Button } from "./Button";
import { API_BASE } from "@/lib/constants";

interface ErrorCardProps {
  message?: string;
  onRetry?: () => void;
}

export function ErrorCard({ message, onRetry }: ErrorCardProps) {
  // Split multi-line messages into lines for readable display
  const lines = (message ?? "An unexpected error occurred.").split("\n").filter(Boolean);
  const headline = lines[0];
  const details = lines.slice(1);

  // Detect "server not running" scenario
  const isConnectionError =
    message?.includes("Cannot reach") ||
    message?.includes("Connection refused") ||
    message?.includes("Failed to fetch");

  return (
    <div style={{
      background: "#0f0f0f",
      border: "1px solid #2a1a1a",
      borderLeft: "3px solid #c94242",
      borderRadius: 12,
      padding: "28px 32px",
      display: "flex",
      flexDirection: "column",
      gap: 20,
    }}>
      {/* Header */}
      <div style={{ display: "flex", alignItems: "flex-start", gap: 14 }}>
        <AlertCircle size={22} color="#c94242" style={{ flexShrink: 0, marginTop: 1 }} />
        <div style={{ flex: 1 }}>
          <p style={{
            fontWeight: 600, fontSize: 15,
            color: "#f5f0e8", fontFamily: "var(--font-body)",
            margin: "0 0 6px",
          }}>
            {isConnectionError ? "API Server Unreachable" : "Error Loading Data"}
          </p>
          <p style={{
            fontSize: 13.5, color: "#c9c3b8",
            fontFamily: "var(--font-body)", margin: 0, lineHeight: 1.5,
          }}>
            {headline}
          </p>
        </div>
      </div>

      {/* Detail lines (instructions, hints) */}
      {details.length > 0 && (
        <div style={{
          background: "#0a0a0a",
          border: "1px solid #1e1e1e",
          borderRadius: 8,
          padding: "14px 16px",
          display: "flex",
          flexDirection: "column",
          gap: 6,
        }}>
          {details.map((line, i) => {
            const isCommand = line.trim().startsWith("./") || line.trim().startsWith("npm") || line.trim().startsWith("PORT");
            return (
              <p key={i} style={{
                fontSize: 13,
                fontFamily: isCommand ? "var(--font-mono)" : "var(--font-body)",
                color: isCommand ? "#c9a842" : "#6b6560",
                margin: 0,
                lineHeight: 1.6,
                paddingLeft: isCommand ? 8 : 0,
                borderLeft: isCommand ? "2px solid #2a2a2a" : "none",
              }}>
                {line}
              </p>
            );
          })}
        </div>
      )}

      {/* Connection error: show the configured API base */}
      {isConnectionError && (
        <div style={{
          display: "flex", alignItems: "center", gap: 10,
          background: "#111", border: "1px solid #1e1e1e",
          borderRadius: 8, padding: "10px 14px",
        }}>
          <Terminal size={14} color="#6b6560" style={{ flexShrink: 0 }} />
          <span style={{ fontSize: 12, fontFamily: "var(--font-body)", color: "#6b6560" }}>
            Configured API base:&nbsp;
          </span>
          <code style={{
            fontSize: 12, fontFamily: "var(--font-mono)",
            color: "#c9c3b8", background: "#1a1a1a",
            padding: "2px 8px", borderRadius: 4,
          }}>
            {API_BASE}
          </code>
        </div>
      )}

      {/* Retry */}
      {onRetry && (
        <div>
          <Button variant="secondary" size="sm" onClick={onRetry}>
            <RefreshCw size={13} />
            Retry
          </Button>
        </div>
      )}
    </div>
  );
}
