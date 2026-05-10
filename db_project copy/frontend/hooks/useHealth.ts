"use client";
import useSWR from "swr";
import { swrFetcher } from "@/lib/api";
import type { HealthResponse } from "@/lib/types";

export function useHealth() {
  const { data, error, isLoading, mutate } = useSWR<HealthResponse>(
    "/api/health",
    swrFetcher,
    { refreshInterval: 5000 }
  );
  return { health: data, error, isLoading, mutate };
}
