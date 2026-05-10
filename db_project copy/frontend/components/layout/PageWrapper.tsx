"use client";
import { useEffect, useState } from "react";
import { timeAgoFromMs } from "@/lib/utils";

interface PageWrapperProps {
  title: string;
  subtitle?: string;
  children: React.ReactNode;
  updatedAt?: number;
}

export function PageWrapper({ title, subtitle, children, updatedAt }: PageWrapperProps) {
  const [ago, setAgo] = useState("");

  useEffect(() => {
    if (!updatedAt) return;
    setAgo(timeAgoFromMs(updatedAt));
    const id = setInterval(() => setAgo(timeAgoFromMs(updatedAt)), 5000);
    return () => clearInterval(id);
  }, [updatedAt]);

  return (
    <main style={{ minHeight: "100vh", paddingTop: 64, background: "#0a0a0a" }}>
      <div style={{ maxWidth: 1280, margin: "0 auto", padding: "40px 48px" }} className="page-padding">
        {/* Page header */}
        <div style={{ display: "flex", alignItems: "flex-start", justifyContent: "space-between", marginBottom: 6, gap: 16 }}>
          <h1 style={{
            fontFamily: "var(--font-heading)",
            fontSize: "clamp(26px, 3.5vw, 40px)",
            fontWeight: 400,
            color: "#f5f0e8",
            lineHeight: 1.1,
            letterSpacing: "-0.02em",
          }}>
            {title}
          </h1>
          {updatedAt && ago && (
            <span style={{
              fontSize: 12, color: "#6b6560", fontFamily: "var(--font-body)",
              marginTop: 6, flexShrink: 0, whiteSpace: "nowrap",
            }}>
              Updated {ago}
            </span>
          )}
        </div>

        {subtitle && (
          <p style={{
            fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)",
            marginBottom: 24, marginTop: 4, lineHeight: 1.5,
          }}>
            {subtitle}
          </p>
        )}

        <div style={{ height: 1, background: "#1e1e1e", marginBottom: 28 }} />

        {children}
      </div>
    </main>
  );
}
