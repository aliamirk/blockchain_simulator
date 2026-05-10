"use client";

type Variant = "primary" | "secondary" | "ghost" | "danger";
type Size = "sm" | "md" | "lg";

interface ButtonProps extends React.ButtonHTMLAttributes<HTMLButtonElement> {
  variant?: Variant;
  size?: Size;
  loading?: boolean;
  children: React.ReactNode;
}

const sizeStyles: Record<Size, React.CSSProperties> = {
  sm: { padding: "6px 16px", fontSize: 13 },
  md: { padding: "9px 22px", fontSize: 14 },
  lg: { padding: "12px 32px", fontSize: 15 },
};

const variantStyles: Record<Variant, React.CSSProperties> = {
  primary: { background: "#c96442", color: "#fff", border: "1px solid #c96442", borderRadius: 999 },
  secondary: { background: "transparent", color: "#c9c3b8", border: "1px solid #333333", borderRadius: 8 },
  ghost: { background: "transparent", color: "#c9c3b8", border: "1px solid transparent", borderRadius: 8 },
  danger: { background: "#c94242", color: "#fff", border: "1px solid #c94242", borderRadius: 999 },
};

export function Button({
  variant = "primary", size = "md", loading = false,
  children, style, disabled, onMouseEnter, onMouseLeave, ...props
}: ButtonProps) {
  return (
    <button
      {...props}
      disabled={disabled || loading}
      style={{
        display: "inline-flex", alignItems: "center", justifyContent: "center",
        gap: 8, fontFamily: "var(--font-body)", fontWeight: 500,
        cursor: disabled || loading ? "not-allowed" : "pointer",
        opacity: disabled || loading ? 0.5 : 1,
        transition: "all 200ms ease",
        outline: "none",
        ...sizeStyles[size],
        ...variantStyles[variant],
        ...style,
      }}
      onMouseEnter={(e) => {
        if (!disabled && !loading) {
          if (variant === "primary") (e.currentTarget as HTMLButtonElement).style.background = "#e07050";
          if (variant === "secondary") (e.currentTarget as HTMLButtonElement).style.background = "rgba(255,255,255,0.05)";
        }
        onMouseEnter?.(e);
      }}
      onMouseLeave={(e) => {
        if (variant === "primary") (e.currentTarget as HTMLButtonElement).style.background = "#c96442";
        if (variant === "secondary") (e.currentTarget as HTMLButtonElement).style.background = "transparent";
        onMouseLeave?.(e);
      }}
    >
      {loading ? (
        <>
          <span className="spinner" />
          Processing…
        </>
      ) : children}
    </button>
  );
}
