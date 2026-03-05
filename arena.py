import sys
import os
import chess
import time
from src.engine_wrapper import EngineWrapper

def play_game(white_engine_path, black_engine_path, depth=None):
    board = chess.Board()
    
    # Initialize Engines
    print(f"Initializing White: {os.path.basename(white_engine_path)}")
    white = EngineWrapper(white_engine_path)
    if not white.start():
        print("Failed to start White engine.")
        return

    print(f"Initializing Black: {os.path.basename(black_engine_path)}")
    black = EngineWrapper(black_engine_path)
    if not black.start():
        white.stop()
        print("Failed to start Black engine.")
        return

    # Monkey-patch play method if we want to force specific params or just rely on wrapper default
    # The wrapper currently forces `go depth 12`.
    
    print("\nStarting Match!")
    print(f"White: {os.path.basename(white_engine_path)}")
    print(f"Black: {os.path.basename(black_engine_path)}")
    print("-" * 40)

    try:
        while not board.is_game_over():
            start_time = time.time()
            if board.turn == chess.WHITE:
                move = white.play(board)
                player = "White"
            else:
                move = black.play(board)
                player = "Black"
            duration = time.time() - start_time
            
            if move is None:
                print(f"{player} failed to return a move. Game Over.")
                break
                
            san = board.san(move)
            board.push(move)
            print(f"{board.fullmove_number}. {san} ({player}, {duration:.2f}s)")
            
            # Simple board display
            # print(board)
            
    except KeyboardInterrupt:
        print("\nMatch Aborted!")
    finally:
        white.stop()
        black.stop()

    print("-" * 40)
    print("Game Over")
    print(f"Result: {board.result()}")
    print(f"Reason: {board.outcome().termination if board.outcome() else 'Unknown'}")

if __name__ == "__main__":
    engine_dir = os.path.join(os.path.dirname(__file__), "cpp_engine")
    
    # Get .exe files
    engines = [f for f in os.listdir(engine_dir) if f.endswith(".exe")]
    
    if len(engines) < 2:
        print("Not enough engines found to compare! Need at least 2.")
        sys.exit(1)
        
    print("\nAvailable Engines:")
    for i, e in enumerate(engines):
        print(f"[{i}] {e}")
        
    try:
        w_idx = int(input("\nSelect White Engine ID: "))
        b_idx = int(input("Select Black Engine ID: "))
        
        if 0 <= w_idx < len(engines) and 0 <= b_idx < len(engines):
            play_game(
                os.path.join(engine_dir, engines[w_idx]), 
                os.path.join(engine_dir, engines[b_idx])
            )
        else:
            print("Invalid selection.")
    except ValueError:
        print("Invalid input.")
