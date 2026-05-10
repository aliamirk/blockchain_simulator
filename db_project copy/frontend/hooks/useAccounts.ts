"use client";
import useSWR from "swr";
import { swrFetcher } from "@/lib/api";
import type { AccountsResponse } from "@/lib/types";

export function useAccounts() {
  const { data, error, isLoading, mutate } = useSWR<AccountsResponse>(
    "/api/accounts",
    swrFetcher,
    { refreshInterval: 15000 }
  );
  return { accounts: data?.accounts ?? {}, error, isLoading, mutate };
}
