"use client";
import { useState } from "react";
import { mineBlock } from "@/lib/api";
import type { MineResponse } from "@/lib/types";

export function useMine() {
  const [mining, setMining] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const mine = async (): Promise<MineResponse | null> => {
    setMining(true);
    setError(null);
    try {
      const result = await mineBlock();
      return result;
    } catch (e) {
      setError(e instanceof Error ? e.message : "Mining failed");
      return null;
    } finally {
      setMining(false);
    }
  };

  return { mine, mining, error };
}
