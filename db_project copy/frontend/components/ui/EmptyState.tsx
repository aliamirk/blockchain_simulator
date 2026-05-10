import { Inbox } from "lucide-react";

interface EmptyStateProps {
  title?: string;
  description?: string;
  icon?: React.ReactNode;
}

export function EmptyState({ title = "No data", description, icon }: EmptyStateProps) {
  return (
    <div style={{
      display: "flex", flexDirection: "column", alignItems: "center",
      justifyContent: "center", padding: "64px 24px", gap: 16,
    }}>
      <div style={{ color: "#6b6560", opacity: 0.6 }}>
        {icon ?? <Inbox size={40} strokeWidth={1.5} />}
      </div>
      <div style={{ textAlign: "center" }}>
        <p style={{ fontSize: 15, fontWeight: 500, color: "#c9c3b8", fontFamily: "var(--font-body)", margin: 0 }}>
          {title}
        </p>
        {description && (
          <p style={{ fontSize: 13, color: "#6b6560", fontFamily: "var(--font-body)", marginTop: 6, margin: "6px 0 0" }}>
            {description}
          </p>
        )}
      </div>
    </div>
  );
}
