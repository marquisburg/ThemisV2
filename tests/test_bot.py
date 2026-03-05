import unittest
import chess
from src.evaluation import evaluate_board
from src.bot import ChessBot

class TestChessBot(unittest.TestCase):
    def setUp(self):
        self.bot = ChessBot(depth=3)

    def test_material_evaluation(self):
        board = chess.Board()
        # Initial position evaluation should be 0 (symmetric)
        self.assertEqual(evaluate_board(board), 0)

    def test_mate_in_one(self):
        # Scholar's Mate pattern
        board = chess.Board()
        board.push_san("e4")
        board.push_san("e5")
        board.push_san("Qh5")
        board.push_san("Nc6")
        board.push_san("Bc4")
        board.push_san("Nf6") 
        # White to move, mate in 1: Qxf7#
        # Create a specific board state where mate is inevitable and immediate
        board = chess.Board("r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4")
        
        best_move = self.bot.get_best_move(board)
        self.assertEqual(best_move, chess.Move.from_uci("h5f7"))

    def test_capture_hanging_piece(self):
        # White K at e1, Q at d1. Black K at e8, N at a5.
        # Queen captures knight at a4 (d1-a4 is diagonal: dx=-3, dy=3. Wait d1(3,0)->a4(0,3)).
        # Path: c2(2,1), b3(1,2).
        # Need empty c2, b3.
        # FEN: 4k3/8/8/8/n7/8/8/3QK3 w - - 0 1
        board = chess.Board("4k3/8/8/8/n7/8/8/3QK3 w - - 0 1") 
        best_move = self.bot.get_best_move(board)
        self.assertEqual(best_move, chess.Move.from_uci("d1a4"))

if __name__ == '__main__':
    unittest.main()
