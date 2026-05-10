interface ChartTooltipProps {
  active?: boolean;
  payload?: Array<{ value: number | string; name?: string }>;
  label?: string | number;
  formatter?: (value: number | string) => string;
}

export function ChartTooltip({ active, payload, label, formatter }: ChartTooltipProps) {
  if (!active || !payload?.length) return null;
  return (
    <div style={{
      background: "#1a1a1a",
      border: "1px solid #2a2a2a",
      borderLeft: "3px solid #c96442",
      borderRadius: 8,
      padding: "10px 14px",
      boxShadow: "0 8px 24px rgba(0,0,0,0.4)",
    }}>
      {label !== undefined && (
        <p style={{ fontSize: 11, color: "#6b6560", fontFamily: "var(--font-body)", marginBottom: 4, margin: "0 0 4px" }}>
          {label}
        </p>
      )}
      {payload.map((p, i) => (
        <p key={i} style={{ fontSize: 13, color: "#c9c3b8", fontFamily: "var(--font-body)", margin: 0 }}>
          {p.name && <span style={{ marginRight: 6 }}>{p.name}:</span>}
          <span style={{ color: "#f5f0e8", fontFamily: "var(--font-mono)", fontWeight: 600 }}>
            {formatter ? formatter(p.value) : p.value}
          </span>
        </p>
      ))}
    </div>
  );
}
