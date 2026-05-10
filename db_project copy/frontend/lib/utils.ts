import { formatDistanceToNow } from "date-fns";

export function formatHash(hash: number | string): string {
  const s = String(hash);
  if (s.length <= 16) return s;
  return s.slice(0, 10) + "…" + s.slice(-6);
}

export function formatAmount(amount: number): string {
  return new Intl.NumberFormat("en-US", {
    style: "currency",
    currency: "USD",
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  }).format(amount);
}

export function formatNumber(n: number): string {
  return new Intl.NumberFormat("en-US").format(n);
}

export function timeAgo(dateStr: string): string {
  try {
    const date = new Date(dateStr);
    return formatDistanceToNow(date, { addSuffix: true });
  } catch {
    return dateStr;
  }
}

export function timeAgoFromMs(ms: number): string {
  const seconds = Math.floor((Date.now() - ms) / 1000);
  if (seconds < 5) return "just now";
  if (seconds < 60) return `${seconds}s ago`;
  const minutes = Math.floor(seconds / 60);
  if (minutes < 60) return `${minutes}m ago`;
  const hours = Math.floor(minutes / 60);
  return `${hours}h ago`;
}

export function copyToClipboard(text: string): Promise<void> {
  return navigator.clipboard.writeText(text);
}

export function bucketAmounts(amounts: number[]): { range: string; count: number }[] {
  const buckets = [
    { min: 0, max: 50, label: "$0–50" },
    { min: 50, max: 100, label: "$50–100" },
    { min: 100, max: 250, label: "$100–250" },
    { min: 250, max: 500, label: "$250–500" },
    { min: 500, max: 1000, label: "$500–1k" },
    { min: 1000, max: Infinity, label: "$1k+" },
  ];
  return buckets.map((b) => ({
    range: b.label,
    count: amounts.filter((a) => a >= b.min && a < b.max).length,
  }));
}
