"use client";
import useSWR from "swr";
import { swrFetcher } from "@/lib/api";
import type { BlocksResponse, Block } from "@/lib/types";

export function useBlocks() {
  const { data, error, isLoading, mutate } = useSWR<BlocksResponse>(
    "/api/blockchain/blocks",
    swrFetcher,
    { refreshInterval: 15000 }
  );
  return { blocks: data?.blocks ?? [], total: data?.total ?? 0, error, isLoading, mutate };
}

export function useBlock(index: number | null) {
  const { data, error, isLoading } = useSWR<Block>(
    index !== null ? `/api/blockchain/blocks/${index}` : null,
    swrFetcher
  );
  return { block: data, error, isLoading };
}
