export type ShellPayload = any;

export interface ShellMessage {
  type: string;
  payload: ShellPayload;
}

type Listener<T = any> = (message: T) => void;

export class ShellIPC {
  private static instance: ShellIPC;
  private socket: WebSocket | null = null;
  private listeners: Map<string, Set<Listener>> = new Map();

  private constructor() {}

  static connect(url: string): Promise<ShellIPC> {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(url);

      ws.onopen = () => {
        const instance = this.getInstance();
        instance.socket = ws;
        ws.onmessage = (event) => instance.handleMessage(event);
        resolve(instance);
      };

      ws.onerror = (error) => {
        console.error("WebSocket error:", error);
        reject(new Error("Failed to connect to WebSocket"));
      };

      ws.onclose = () => {
        console.log("WebSocket connection closed.");
        this.getInstance().socket = null;
      };
    });
  }

  public isReady(): boolean {
    return this.socket?.readyState === WebSocket.OPEN;
  }

  public send(type: string, payload: ShellPayload): void {
    if (!this.isReady()) {
      throw new Error("WebSocket is not connected.");
    }

    const message: ShellMessage = { type, payload };
    this.socket!.send(JSON.stringify(message));
  }

  public listen<T>(type: string, callback: Listener<T>): void {
    if (!this.listeners.has(type)) {
      this.listeners.set(type, new Set());
    }
    this.listeners.get(type)!.add(callback as Listener);
  }

  public unlisten<T>(type: string, callback?: Listener<T>): void {
    if (!this.listeners.has(type)) return;

    if (callback) {
      this.listeners.get(type)!.delete(callback as Listener);
    } else {
      this.listeners.delete(type);
    }
  }

  private handleMessage(event: MessageEvent): void {
    try {
      const message: ShellMessage = JSON.parse(event.data);
      const callbacks = this.listeners.get(message.type);
      if (callbacks) {
        for (const cb of callbacks) {
          cb(message.payload);
        }
      }
    } catch (error) {
      console.error("Error parsing WebSocket message:", error);
    }
  }

  public static disconnect(): void {
    const instance = this.getInstance();
    if (instance.socket) {
      instance.socket.close();
      instance.socket = null;
    }
    instance.listeners.clear();
  }

  public static getInstance(): ShellIPC {
    if (!ShellIPC.instance) {
      ShellIPC.instance = new ShellIPC();
    }
    return ShellIPC.instance;
  }
}
