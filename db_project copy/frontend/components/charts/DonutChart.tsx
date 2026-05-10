"use client";
import { PieChart, Pie, Cell, Tooltip, Legend } from "recharts";
import { ChartTooltip } from "./ChartTooltip";
import { ChartContainer } from "./ChartContainer";

interface DonutChartProps {
  data: Array<{ name: string; value: number }>;
  colors?: string[];
  formatter?: (v: number | string) => string;
  height?: number;
}

const DEFAULT_COLORS = [
  "#c96442", "#c9a842", "#4a9e6b", "#4278c9", "#9e4a8e",
  "#42b8c9", "#c94278", "#6b9e4a", "#c97842", "#4a6bc9",
  "#9ec942", "#c94a4a", "#42c9a8", "#8e42c9", "#c9c242",
];

export function DonutChart({ data, colors = DEFAULT_COLORS, formatter, height = 240 }: DonutChartProps) {
  return (
    <ChartContainer height={height}>
      {(width) => (
        <PieChart width={width} height={height}>
          <Pie
            data={data}
            cx="50%"
            cy="50%"
            innerRadius="52%"
            outerRadius="72%"
            paddingAngle={3}
            dataKey="value"
          >
            {data.map((_, i) => (
              <Cell key={i} fill={colors[i % colors.length]} />
            ))}
          </Pie>
          <Tooltip content={<ChartTooltip formatter={formatter} />} />
          <Legend
            iconType="circle"
            iconSize={8}
            formatter={(value) => (
              <span style={{ color: "#c9c3b8", fontSize: 12, fontFamily: "var(--font-body)" }}>
                {value}
              </span>
            )}
          />
        </PieChart>
      )}
    </ChartContainer>
  );
}
