import subprocess
import chess


class EngineWrapper:
    def __init__(self, engine_path):
        self.engine_path = engine_path
        self.process = None

    def start(self):
        try:
            self.process = subprocess.Popen(
                self.engine_path,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                universal_newlines=True,
                bufsize=1,
            )
            # Consume initial output
            self._read_until("uciok")
        except FileNotFoundError:
            print(f"Error: Engine executable not found at {self.engine_path}")
            return False
        except Exception as e:
            print(f"Error starting engine: {e}")
            return False
        return True

    def stop(self):
        if self.process:
            self._send_command("quit")
            self.process.communicate()
            self.process = None

    def _send_command(self, command):
        if self.process:
            self.process.stdin.write(command + "\n")
            self.process.stdin.flush()

    def _read_until(self, token):
        while True:
            line = self.process.stdout.readline().strip()
            if token in line:
                return line
            if not line and self.process.poll() is not None:
                break
        return None

    def play(self, board, time_limit=10.0, quiet=False):
        fen = board.fen()
        self._send_command(f"position fen {fen}")
        self._send_command("go depth 15")

        while True:
            line = self.process.stdout.readline().strip()
            if line and not quiet:
                print(line)
            if line.startswith("bestmove"):
                parts = line.split()
                if len(parts) >= 2:
                    move_str = parts[1]
                    return chess.Move.from_uci(move_str)
                break
        return None


if __name__ == "__main__":
    import os

    engine_path = os.path.join(
        os.path.dirname(__file__), "..", "cpp_engine", "ThemisEngine.exe"
    )
    engine = EngineWrapper(engine_path)
    if engine.start():
        board = chess.Board()
        move = engine.play(board)
        print(f"Engine played: {move}")
        engine.stop()
