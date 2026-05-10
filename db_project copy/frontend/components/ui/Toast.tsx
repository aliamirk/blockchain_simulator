"use client";
import { createContext, useContext, useState, useCallback } from "react";
import { CheckCircle, XCircle, AlertCircle, X } from "lucide-react";

type ToastType = "success" | "error" | "warning";
interface ToastItem { id: number; type: ToastType; message: string; exiting?: boolean; }
interface ToastContextValue { toast: (type: ToastType, message: string) => void; }

const ToastContext = createContext<ToastContextValue>({ toast: () => {} });
export function useToast() { return useContext(ToastContext); }

let _id = 0;

export function ToastProvider({ children }: { children: React.ReactNode }) {
  const [toasts, setToasts] = useState<ToastItem[]>([]);

  const dismiss = useCallback((id: number) => {
    setToasts(prev => prev.map(t => t.id === id ? { ...t, exiting: true } : t));
    setTimeout(() => setToasts(prev => prev.filter(t => t.id !== id)), 350);
  }, []);

  const toast = useCallback((type: ToastType, message: string) => {
    const id = ++_id;
    setToasts(prev => [...prev, { id, type, message }]);
    setTimeout(() => dismiss(id), 4000);
  }, [dismiss]);

  const iconMap: Record<ToastType, React.ReactNode> = {
    success: <CheckCircle size={16} color="#4a9e6b" />,
    error:   <XCircle    size={16} color="#c94242" />,
    warning: <AlertCircle size={16} color="#c9a842" />,
  };

  return (
    <ToastContext.Provider value={{ toast }}>
      {children}
      <div
        aria-live="polite"
        aria-label="Notifications"
        style={{
          position: "fixed", bottom: 24, right: 24,
          display: "flex", flexDirection: "column", gap: 12,
          zIndex: 9999, pointerEvents: "none",
        }}
      >
        {toasts.map(t => (
          <div
            key={t.id}
            role="alert"
            className={t.exiting ? "toast-exit" : "toast-enter"}
            style={{
              display: "flex", alignItems: "flex-start", gap: 12,
              background: "#1a1a1a", border: "1px solid #2a2a2a",
              borderRadius: 12, padding: "14px 16px",
              minWidth: 300, maxWidth: 400,
              boxShadow: "0 8px 32px rgba(0,0,0,0.5)",
              pointerEvents: "all",
            }}
          >
            <span style={{ flexShrink: 0, marginTop: 1 }}>{iconMap[t.type]}</span>
            <p style={{ fontSize: 14, color: "#c9c3b8", fontFamily: "var(--font-body)", flex: 1, margin: 0, lineHeight: 1.5 }}>
              {t.message}
            </p>
            <button
              onClick={() => dismiss(t.id)}
              aria-label="Dismiss"
              style={{ background: "none", border: "none", cursor: "pointer", color: "#6b6560", padding: 2, flexShrink: 0 }}
            >
              <X size={14} />
            </button>
          </div>
        ))}
      </div>
    </ToastContext.Provider>
  );
}
