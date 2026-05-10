interface SkeletonProps {
  height?: number | string;
  width?: number | string;
  style?: React.CSSProperties;
}

export function Skeleton({ height = 16, width = "100%", style }: SkeletonProps) {
  return (
    <div className="skeleton" style={{ height, width, borderRadius: 6, ...style }} />
  );
}

export function SkeletonCard({ rows = 3 }: { rows?: number }) {
  return (
    <div style={{
      background: "#111111", border: "1px solid #222222",
      borderRadius: 12, padding: 24,
      display: "flex", flexDirection: "column", gap: 16,
    }}>
      <Skeleton height={20} width="40%" />
      {Array.from({ length: rows }).map((_, i) => (
        <Skeleton key={i} height={14} width={`${65 + (i % 3) * 12}%`} />
      ))}
    </div>
  );
}

export function SkeletonTable({ rows = 5 }: { rows?: number }) {
  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 8 }}>
      {Array.from({ length: rows }).map((_, i) => (
        <Skeleton key={i} height={48} />
      ))}
    </div>
  );
}
