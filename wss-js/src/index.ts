export type ShellPayload = any;

export interface ShellMessage {
  type: string;
  payload: ShellPayload;
}

export class ShellIPC {
  private static instance: ShellIPC;
  private socket: WebSocket | null = null;

  private constructor() {}

  static connect(url: string): Promise<ShellIPC> {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(url);

      ws.onopen = () => {
        this.getInstance().socket = ws;
        resolve(this.getInstance());
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

  public send(type: string, payload: ShellPayload): void {
    if (!this.socket) {
      throw new Error("WebSocket is not connected.");
    }
    const message: ShellMessage = { type, payload };
    this.socket.send(JSON.stringify(message));
  }

  public listen<T>(type: string, callback: (message: T) => void): void {
    if (!this.socket) {
      throw new Error("WebSocket is not connected.");
    }
    this.socket.onmessage = (event) => {
      try {
        const message: ShellMessage = JSON.parse(event.data);
        if (message.type === type) {
          callback(message.payload as T);
        }
      } catch (error) {
        console.error("Error parsing message:", error);
      }
    };
  }

  public static disconnect(): void {
    const instance = ShellIPC.getInstance();
    if (instance.socket) {
      instance.socket.close();
      instance.socket = null;
    }
  }

  public static getInstance(): ShellIPC {
    if (!ShellIPC.instance) {
      ShellIPC.instance = new ShellIPC();
    }
    return ShellIPC.instance;
  }
}
