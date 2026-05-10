"use client";
import {
  BarChart as ReBarChart, Bar, XAxis, YAxis, CartesianGrid,
  Tooltip,
} from "recharts";
import { ChartTooltip } from "./ChartTooltip";
import { ChartContainer } from "./ChartContainer";

interface BarChartProps {
  data: Array<Record<string, string | number>>;
  xKey: string;
  yKey: string;
  color?: string;
  formatter?: (v: number | string) => string;
  height?: number;
  layout?: "horizontal" | "vertical";
}

export function BarChart({ data, xKey, yKey, color = "#c96442", formatter, height = 260, layout = "vertical" }: BarChartProps) {
  return (
    <ChartContainer height={height}>
      {(width) => (
        <ReBarChart
          width={width}
          height={height}
          data={data}
          layout={layout}
          margin={{ top: 4, right: 8, left: 0, bottom: 4 }}
        >
          <CartesianGrid
            horizontal={layout === "vertical"}
            vertical={layout === "horizontal"}
            stroke="#222222"
            strokeDasharray="4 4"
          />
          {layout === "vertical" ? (
            <>
              <XAxis
                dataKey={xKey}
                tick={{ fill: "#6b6560", fontSize: 11, fontFamily: "var(--font-mono)" }}
                axisLine={false}
                tickLine={false}
              />
              <YAxis
                tick={{ fill: "#6b6560", fontSize: 11, fontFamily: "var(--font-mono)" }}
                axisLine={false}
                tickLine={false}
                width={36}
              />
            </>
          ) : (
            <>
              <XAxis
                type="number"
                tick={{ fill: "#6b6560", fontSize: 11, fontFamily: "var(--font-mono)" }}
                axisLine={false}
                tickLine={false}
              />
              <YAxis
                dataKey={xKey}
                type="category"
                tick={{ fill: "#6b6560", fontSize: 11, fontFamily: "var(--font-body)" }}
                axisLine={false}
                tickLine={false}
                width={72}
              />
            </>
          )}
          <Tooltip
            content={<ChartTooltip formatter={formatter} />}
            cursor={{ fill: "rgba(245,240,232,0.04)" }}
          />
          <Bar dataKey={yKey} fill={color} radius={4} maxBarSize={36} />
        </ReBarChart>
      )}
    </ChartContainer>
  );
}
