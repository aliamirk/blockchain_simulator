"use client";
import useSWR from "swr";
import { swrFetcher } from "@/lib/api";
import type { PendingResponse } from "@/lib/types";

export function usePending() {
  const { data, error, isLoading, mutate } = useSWR<PendingResponse>(
    "/api/transactions/pending",
    swrFetcher,
    { refreshInterval: 5000 }
  );
  return {
    pending: data?.pending ?? [],
    count: data?.count ?? 0,
    error,
    isLoading,
    mutate,
  };
}
