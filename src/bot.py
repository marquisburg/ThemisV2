import chess
from .search import minimax, iterative_deepening

class ChessBot:
    def __init__(self, depth=3):
        self.depth = depth

    def get_best_move(self, board):
        """
        Determines the best move for the current player on the board.
        Uses Iterative Deepening with a time limit.
        """
        # Default fixed depth is ignored in favor of time_limit for now,
        # or we could make iterative_deepening take max_depth too.
        # Let's use 2.0s as standard for "slightly increasing depth" feel.
        return iterative_deepening(board, time_limit=2.0)
