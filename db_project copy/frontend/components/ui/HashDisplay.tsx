"use client";
import { useState } from "react";
import { Copy, Check } from "lucide-react";
import { copyToClipboard } from "@/lib/utils";

interface HashDisplayProps {
  hash: number | string;
}

export function HashDisplay({ hash }: HashDisplayProps) {
  const [copied, setCopied] = useState(false);
  const s = String(hash);
  const display = s.length > 16 ? s.slice(0, 10) + "…" + s.slice(-6) : s;

  const handleCopy = async () => {
    await copyToClipboard(s);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <span
      title={s}
      style={{ display: "inline-flex", alignItems: "center", gap: 6 }}
      className="hash-display-group"
    >
      <span style={{
        fontFamily: "var(--font-mono)", fontSize: 13,
        color: "#c9c3b8", letterSpacing: "0.02em",
      }}>
        {display}
      </span>
      <button
        onClick={handleCopy}
        aria-label={copied ? "Copied" : "Copy hash"}
        style={{
          background: "none", border: "none", cursor: "pointer", padding: 2,
          color: copied ? "#4a9e6b" : "#6b6560",
          opacity: 0, transition: "opacity 200ms",
          display: "inline-flex", alignItems: "center",
        }}
        className="hash-copy-btn"
      >
        {copied ? <Check size={12} /> : <Copy size={12} />}
      </button>
      <style>{`.hash-display-group:hover .hash-copy-btn { opacity: 1 !important; }`}</style>
    </span>
  );
}
