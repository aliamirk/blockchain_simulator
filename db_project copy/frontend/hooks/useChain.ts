"use client";
import useSWR from "swr";
import { swrFetcher } from "@/lib/api";
import type { SummaryResponse, ChainsResponse, VerifyResponse } from "@/lib/types";

export function useSummary() {
  const { data, error, isLoading, mutate } = useSWR<SummaryResponse>(
    "/api/blockchain/summary",
    swrFetcher,
    { refreshInterval: 15000 }
  );
  return { summary: data, error, isLoading, mutate };
}

export function useChains() {
  const { data, error, isLoading, mutate } = useSWR<ChainsResponse>(
    "/api/blockchain/chains",
    swrFetcher,
    { refreshInterval: 15000 }
  );
  return { chains: data?.chains ?? [], error, isLoading, mutate };
}

export function useVerify() {
  const { data, error, isLoading, mutate } = useSWR<VerifyResponse>(
    "/api/blockchain/verify",
    swrFetcher,
    { revalidateOnFocus: false }
  );
  return { verify: data, error, isLoading, mutate };
}
