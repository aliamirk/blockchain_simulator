import type { Metadata } from "next";
import "./globals.css";
import { Navbar } from "@/components/layout/Navbar";
import { Footer } from "@/components/layout/Footer";
import { ToastProvider } from "@/components/ui/Toast";

export const metadata: Metadata = {
  title: "Blockchain Dashboard",
  description: "Production-grade blockchain explorer and management dashboard",
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en" style={{ height: "100%" }}>
      <body style={{
        minHeight: "100%", display: "flex", flexDirection: "column",
        background: "#0a0a0a", margin: 0, padding: 0,
        WebkitFontSmoothing: "antialiased",
      }}>
        <ToastProvider>
          <Navbar />
          <div style={{ flex: 1, display: "flex", flexDirection: "column" }}>
            {children}
          </div>
          <Footer />
        </ToastProvider>
      </body>
    </html>
  );
}
