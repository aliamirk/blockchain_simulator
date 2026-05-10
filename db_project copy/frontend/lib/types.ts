// Types derived from actual blockchain_api.cpp responses

export interface HealthResponse {
  status: string;
  dbConnected: boolean;
  chainLoaded: boolean;
  chainName: string;
}

export interface Transaction {
  id: number;
  sender: string;
  receiver: string;
  amount: number;
  metadata: string;
  signature: string;
}

export interface Block {
  index: number;
  hash: number;
  prevHash: number;
  timestamp: string;
  nonce: number;
  transactions: Transaction[];
}

export interface BlocksResponse {
  blocks: Block[];
  total: number;
}

export interface SummaryResponse {
  chainName: string;
  totalBlocks: number;
  difficulty: number;
  latestHash: number;
  totalTransactions: number;
  pendingCount: number;
}

export interface VerifyResponse {
  valid: boolean;
  message: string;
}

export interface PendingResponse {
  pending: Transaction[];
  count: number;
}

export interface AccountsResponse {
  accounts: Record<string, number>;
}

export interface Chain {
  name: string;
  difficulty: number;
  totalBlocks: number;
  totalTx: number;
  createdAt: string;
}

export interface ChainsResponse {
  chains: Chain[];
}

export interface CreateChainResponse {
  success: boolean;
  message: string;
  chainName: string;
  difficulty: number;
  defaultAccounts: string[];
}

export interface LoadChainResponse {
  success: boolean;
  message: string;
  chainName: string;
  difficulty: number;
  blocks: number;
}

export interface SaveChainResponse {
  success: boolean;
  message: string;
}

export interface AddTransactionResponse {
  success: boolean;
  transaction: Transaction;
  pendingCount: number;
}

export interface MineResponse {
  success: boolean;
  message: string;
  block: Block;
  totalBlocks: number;
}

export interface SearchTxResponse {
  found: boolean;
  transaction: Transaction & { blockIndex: number; positionInBlock: number };
}

export interface SearchListResponse {
  results: Array<Transaction & { blockIndex: number }>;
  count: number;
}

export interface ApiError {
  error: string;
}
