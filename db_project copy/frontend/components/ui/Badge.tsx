type Variant = "success" | "danger" | "warning" | "info" | "default";

interface BadgeProps {
  variant?: Variant;
  children: React.ReactNode;
}

const styles: Record<Variant, { bg: string; color: string }> = {
  success: { bg: "rgba(74,158,107,0.15)",  color: "#4a9e6b" },
  danger:  { bg: "rgba(201,66,66,0.15)",   color: "#c94242" },
  warning: { bg: "rgba(201,168,66,0.15)",  color: "#c9a842" },
  info:    { bg: "rgba(66,120,201,0.15)",  color: "#4278c9" },
  default: { bg: "rgba(245,240,232,0.07)", color: "#c9c3b8" },
};

export function Badge({ variant = "default", children }: BadgeProps) {
  const s = styles[variant];
  return (
    <span style={{
      display: "inline-flex", alignItems: "center", gap: 4,
      padding: "2px 10px", borderRadius: 999,
      fontSize: 12, fontWeight: 500, fontFamily: "var(--font-body)",
      background: s.bg, color: s.color,
      whiteSpace: "nowrap",
    }}>
      {children}
    </span>
  );
}
