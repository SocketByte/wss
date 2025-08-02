import WebSocket from "ws";

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

      ws.on("open", () => {
        ShellIPC.getInstance().socket = ws;
        resolve(ShellIPC.getInstance());
      });

      ws.on("error", (error) => {
        reject(new Error(`WebSocket connection error: ${error.message}`));
      });

      ws.on("close", () => {
        ShellIPC.getInstance().socket = null;
      });
    });
  }

  public sendMessage(type: string, payload: ShellPayload): void {
    if (!this.socket) {
      throw new Error("WebSocket is not connected.");
    }
    const message: ShellMessage = { type, payload };
    this.socket.send(JSON.stringify(message), (error) => {
      if (error) {
        console.error("Error sending message:", error);
      }
    });
  }

  public onMessage<T>(type: string, callback: (message: T) => void): void {
    if (!this.socket) {
      throw new Error("WebSocket is not connected.");
    }
    this.socket.on("message", (data: WebSocket.Data) => {
      try {
        const message: ShellMessage = JSON.parse(data.toString());
        if (message.type === type) {
          callback(message.payload as T);
        }
      } catch (error) {
        console.error("Error parsing message:", error);
      }
    });
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
