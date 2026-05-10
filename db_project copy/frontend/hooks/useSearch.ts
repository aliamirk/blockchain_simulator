"use client";
import { useState } from "react";
import { searchBySender, searchByReceiver, searchByTxId } from "@/lib/api";
import type { SearchListResponse, SearchTxResponse } from "@/lib/types";

export type SearchMode = "sender" | "receiver" | "txid";

export function useSearch() {
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [listResults, setListResults] = useState<SearchListResponse | null>(null);
  const [txResult, setTxResult] = useState<SearchTxResponse | null>(null);

  const search = async (mode: SearchMode, query: string) => {
    setLoading(true);
    setError(null);
    setListResults(null);
    setTxResult(null);
    try {
      if (mode === "sender") {
        const r = await searchBySender(query);
        setListResults(r);
      } else if (mode === "receiver") {
        const r = await searchByReceiver(query);
        setListResults(r);
      } else {
        const id = parseInt(query, 10);
        if (isNaN(id)) throw new Error("TX ID must be a number");
        const r = await searchByTxId(id);
        setTxResult(r);
      }
    } catch (e) {
      setError(e instanceof Error ? e.message : "Search failed");
    } finally {
      setLoading(false);
    }
  };

  return { search, loading, error, listResults, txResult };
}
