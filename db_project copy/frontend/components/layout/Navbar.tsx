"use client";
import { useState, useEffect } from "react";
import Link from "next/link";
import { usePathname } from "next/navigation";
import { Hexagon, Menu, X } from "lucide-react";
import { useHealth } from "@/hooks/useHealth";
import { useSummary } from "@/hooks/useChain";

const NAV_LINKS = [
  { href: "/dashboard",    label: "Overview" },
  { href: "/blocks",       label: "Blocks" },
  { href: "/transactions", label: "Transactions" },
  { href: "/mine",         label: "Mine" },
  { href: "/accounts",     label: "Accounts" },
  { href: "/search",       label: "Search" },
  { href: "/chains",       label: "Chains" },
];

export function Navbar() {
  const pathname = usePathname();
  const { health, error: healthError } = useHealth();
  const { summary } = useSummary();
  const [mobileOpen, setMobileOpen] = useState(false);
  const [lastHealthTime, setLastHealthTime] = useState(Date.now());
  const [hoveredLink, setHoveredLink] = useState<string | null>(null);
  const [scrolled, setScrolled] = useState(false);

  useEffect(() => {
    if (health) setLastHealthTime(Date.now());
  }, [health]);

  useEffect(() => {
    const onScroll = () => setScrolled(window.scrollY > 8);
    window.addEventListener("scroll", onScroll, { passive: true });
    return () => window.removeEventListener("scroll", onScroll);
  }, []);

  const stale = Date.now() - lastHealthTime > 15000;
  const liveColor = healthError ? "#c94242" : stale ? "#c9a842" : "#4a9e6b";
  const chainName = summary?.chainName || "BlockChain";

  return (
    <>
      <nav style={{
        position: "fixed", top: 0, left: 0, right: 0, zIndex: 40,
        height: 60,
        background: scrolled ? "rgba(10,10,10,0.96)" : "rgba(10,10,10,0.88)",
        backdropFilter: "blur(20px)",
        WebkitBackdropFilter: "blur(20px)",
        borderBottom: `1px solid ${scrolled ? "#1e1e1e" : "#181818"}`,
        transition: "background 200ms ease, border-color 200ms ease",
      }}>
        <div style={{
          height: "100%",
          maxWidth: 1280,
          margin: "0 auto",
          padding: "0 48px",
          display: "flex",
          alignItems: "center",
          justifyContent: "space-between",
          gap: 24,
        }} className="nav-padding">

          {/* ── Logo ─────────────────────────────────────────────────── */}
          <Link href="/dashboard" style={{
            display: "flex", alignItems: "center", gap: 9,
            textDecoration: "none", color: "#f5f0e8",
            fontFamily: "var(--font-body)", fontWeight: 600, fontSize: 14,
            letterSpacing: "-0.01em", flexShrink: 0,
          }}>
            <Hexagon size={20} color="#c96442" strokeWidth={1.5} />
            <span>{chainName}</span>
          </Link>

          {/* ── Desktop Nav ──────────────────────────────────────────── */}
          <div style={{ display: "flex", alignItems: "center", gap: 2, flex: 1, justifyContent: "center" }}>
            {NAV_LINKS.map((link) => {
              const active = pathname === link.href || pathname.startsWith(link.href + "/");
              const hovered = hoveredLink === link.href;
              return (
                <Link
                  key={link.href}
                  href={link.href}
                  onMouseEnter={() => setHoveredLink(link.href)}
                  onMouseLeave={() => setHoveredLink(null)}
                  style={{
                    padding: "5px 13px",
                    fontSize: 13.5,
                    fontFamily: "var(--font-body)",
                    fontWeight: active ? 500 : 400,
                    color: active ? "#f5f0e8" : hovered ? "#c9c3b8" : "#6b6560",
                    textDecoration: "none",
                    borderRadius: 6,
                    background: active
                      ? "rgba(201,100,66,0.1)"
                      : hovered
                      ? "rgba(255,255,255,0.04)"
                      : "transparent",
                    borderBottom: active ? "2px solid #c96442" : "2px solid transparent",
                    transition: "all 150ms ease",
                    display: "block",
                    whiteSpace: "nowrap",
                  }}
                >
                  {link.label}
                </Link>
              );
            })}
          </div>

          {/* ── Right: Live indicator ────────────────────────────────── */}
          <div style={{ display: "flex", alignItems: "center", gap: 16, flexShrink: 0 }}>
            <div style={{ display: "flex", alignItems: "center", gap: 7 }}>
              <span
                className="pulse"
                style={{
                  display: "inline-block", width: 7, height: 7,
                  borderRadius: "50%", background: liveColor,
                  boxShadow: `0 0 6px ${liveColor}`,
                }}
                aria-label={healthError ? "API offline" : stale ? "Stale" : "Live"}
              />
              <span style={{
                fontSize: 11, fontWeight: 700, letterSpacing: "0.12em",
                color: "#6b6560", fontFamily: "var(--font-body)",
                textTransform: "uppercase",
              }}>
                Live
              </span>
            </div>

            {/* Mobile hamburger */}
            <button
              onClick={() => setMobileOpen(true)}
              aria-label="Open menu"
              className="mobile-menu-btn"
              style={{
                display: "none",
                background: "none", border: "none",
                cursor: "pointer", color: "#c9c3b8", padding: 4,
              }}
            >
              <Menu size={20} />
            </button>
          </div>
        </div>
      </nav>

      {/* ── Mobile Drawer ────────────────────────────────────────────── */}
      {mobileOpen && (
        <div style={{ position: "fixed", inset: 0, zIndex: 50 }}>
          <div
            style={{ position: "absolute", inset: 0, background: "rgba(0,0,0,0.8)" }}
            onClick={() => setMobileOpen(false)}
          />
          <div style={{
            position: "absolute", top: 0, right: 0, bottom: 0, width: 280,
            background: "#111111", borderLeft: "1px solid #1e1e1e",
            display: "flex", flexDirection: "column",
            animation: "slide-in-right 240ms cubic-bezier(0.34,1.2,0.64,1)",
          }}>
            <div style={{
              display: "flex", alignItems: "center", justifyContent: "space-between",
              padding: "18px 24px", borderBottom: "1px solid #1e1e1e",
            }}>
              <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
                <Hexagon size={18} color="#c96442" strokeWidth={1.5} />
                <span style={{ color: "#f5f0e8", fontWeight: 600, fontFamily: "var(--font-body)", fontSize: 14 }}>
                  {chainName}
                </span>
              </div>
              <button
                onClick={() => setMobileOpen(false)}
                style={{ background: "none", border: "none", cursor: "pointer", color: "#6b6560", padding: 4 }}
              >
                <X size={18} />
              </button>
            </div>
            <nav style={{ display: "flex", flexDirection: "column", padding: "12px 12px", gap: 2 }}>
              {NAV_LINKS.map((link) => {
                const active = pathname === link.href;
                return (
                  <Link
                    key={link.href}
                    href={link.href}
                    onClick={() => setMobileOpen(false)}
                    style={{
                      padding: "11px 16px", borderRadius: 8, fontSize: 14,
                      fontFamily: "var(--font-body)", fontWeight: active ? 500 : 400,
                      color: active ? "#f5f0e8" : "#c9c3b8",
                      background: active ? "rgba(201,100,66,0.1)" : "transparent",
                      textDecoration: "none", transition: "all 150ms ease",
                      borderLeft: active ? "2px solid #c96442" : "2px solid transparent",
                    }}
                  >
                    {link.label}
                  </Link>
                );
              })}
            </nav>
          </div>
        </div>
      )}

      <style>{`
        @media (max-width: 768px) {
          .mobile-menu-btn { display: block !important; }
        }
      `}</style>
    </>
  );
}
