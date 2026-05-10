"use client";
import { useRef, useState, useEffect } from "react";

interface ChartContainerProps {
  height: number;
  children: (width: number) => React.ReactNode;
}

/**
 * Measures its own width via ResizeObserver and passes it to children.
 * This replaces ResponsiveContainer which has a first-render blank issue in Recharts v3 + React 19.
 */
export function ChartContainer({ height, children }: ChartContainerProps) {
  const ref = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(0);

  useEffect(() => {
    if (!ref.current) return;
    // Measure immediately
    setWidth(ref.current.getBoundingClientRect().width);

    const observer = new ResizeObserver((entries) => {
      const entry = entries[0];
      if (entry) setWidth(Math.round(entry.contentRect.width));
    });
    observer.observe(ref.current);
    return () => observer.disconnect();
  }, []);

  return (
    <div ref={ref} style={{ width: "100%", height }}>
      {width > 0 && children(width)}
    </div>
  );
}
