"use client";
import {
  AreaChart as ReAreaChart, Area, XAxis, YAxis, CartesianGrid,
  Tooltip,
} from "recharts";
import { ChartTooltip } from "./ChartTooltip";
import { ChartContainer } from "./ChartContainer";

interface AreaChartProps {
  data: Array<Record<string, string | number>>;
  xKey: string;
  yKey: string;
  color?: string;
  formatter?: (v: number | string) => string;
  height?: number;
}

export function AreaChart({ data, xKey, yKey, color = "#c96442", formatter, height = 220 }: AreaChartProps) {
  const gradId = `grad-${yKey}-${color.replace(/[^a-z0-9]/gi, "")}`;
  return (
    <ChartContainer height={height}>
      {(width) => (
        <ReAreaChart
          width={width}
          height={height}
          data={data}
          margin={{ top: 4, right: 8, left: 0, bottom: 4 }}
        >
          <defs>
            <linearGradient id={gradId} x1="0" y1="0" x2="0" y2="1">
              <stop offset="5%" stopColor={color} stopOpacity={0.25} />
              <stop offset="95%" stopColor={color} stopOpacity={0} />
            </linearGradient>
          </defs>
          <CartesianGrid horizontal vertical={false} stroke="#222222" strokeDasharray="4 4" />
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
          <Tooltip
            content={<ChartTooltip formatter={formatter} />}
            cursor={{ stroke: "#333", strokeWidth: 1 }}
          />
          <Area
            type="monotone"
            dataKey={yKey}
            stroke={color}
            strokeWidth={2}
            fill={`url(#${gradId})`}
            dot={false}
            activeDot={{ r: 4, fill: color, stroke: "#0a0a0a", strokeWidth: 2 }}
          />
        </ReAreaChart>
      )}
    </ChartContainer>
  );
}
