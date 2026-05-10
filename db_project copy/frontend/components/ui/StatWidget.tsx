"use client";
import { useState } from "react";

interface StatWidgetProps {
  label: string;
  value: string | number;
  mono?: boolean;
  icon?: React.ReactNode;
  delta?: { value: string; positive: boolean };
}

export function StatWidget({ label, value, mono = false, icon, delta }: StatWidgetProps) {
  const [hovered, setHovered] = useState(false);

  return (
    <div
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      style={{
        background: "#111111",
        border: "1px solid #222222",
        borderLeft: hovered ? "3px solid #c96442" : "3px solid transparent",
        borderRadius: 12,
        padding: "18px 20px",
        display: "flex",
        flexDirection: "column",
        gap: 10,
        transition: "border-color 200ms ease, background 200ms ease",
        cursor: "default",
      }}
    >
      {/* Label row */}
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between" }}>
        <span style={{
          fontSize: 11, fontWeight: 600, letterSpacing: "0.08em",
          textTransform: "uppercase", color: "#6b6560",
          fontFamily: "var(--font-body)",
        }}>
          {label}
        </span>
        {icon && (
          <span style={{ color: hovered ? "#c96442" : "#6b6560", transition: "color 200ms" }}>
            {icon}
          </span>
        )}
      </div>

      {/* Value row */}
      <div style={{ display: "flex", alignItems: "flex-end", justifyContent: "space-between", gap: 8 }}>
        <span style={{
          fontSize: 26,
          fontWeight: 600,
          color: "#f5f0e8",
          lineHeight: 1,
          fontFamily: mono ? "var(--font-mono)" : "var(--font-body)",
          wordBreak: "break-all",
          letterSpacing: mono ? "0.02em" : "-0.01em",
        }}>
          {value}
        </span>
        {delta && (
          <span style={{
            fontSize: 11, padding: "3px 8px", borderRadius: 999, fontWeight: 600,
            background: delta.positive ? "rgba(74,158,107,0.15)" : "rgba(201,66,66,0.15)",
            color: delta.positive ? "#4a9e6b" : "#c94242",
            flexShrink: 0, fontFamily: "var(--font-body)",
          }}>
            {delta.value}
          </span>
        )}
      </div>
    </div>
  );
}
