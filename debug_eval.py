import chess
from src.evaluation import evaluate_board

board = chess.Board()
print(f"Evaluation: {evaluate_board(board)}")

# Detailed breakdown
# ... I could add print statements in evaluation.py but let's see the total first.
