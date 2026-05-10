interface CardProps {
  children: React.ReactNode;
  style?: React.CSSProperties;
  padding?: "none" | "sm" | "md" | "lg";
  className?: string;
  onClick?: () => void;
}

const padMap = { none: 0, sm: 16, md: 24, lg: 32 };

export function Card({ children, style, padding = "md", className, onClick }: CardProps) {
  return (
    <div
      className={className}
      onClick={onClick}
      style={{
        background: "#111111",
        border: "1px solid #1e1e1e",
        borderRadius: 12,
        padding: padMap[padding],
        cursor: onClick ? "pointer" : undefined,
        transition: onClick ? "border-color 150ms ease" : undefined,
        ...style,
      }}
    >
      {children}
    </div>
  );
}
