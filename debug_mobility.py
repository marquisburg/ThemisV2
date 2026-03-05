import chess
from src.evaluation import evaluate_board

# We need to copy-paste evaluate_board or similar logic to debug, 
# or just instrument evaluation.py temporarily.
# For now, let's assume I can read the components if I split them.

# Actually, I can just modify evaluation.py to print debug info if a flag is set?
# No, easier to just copy the body here for inspection.

board = chess.Board()
PIECE_VALUES = {
    chess.PAWN: 100,
    chess.KNIGHT: 320,
    chess.BISHOP: 330,
    chess.ROOK: 500,
    chess.QUEEN: 900,
    chess.KING: 20000
}
# PST tables (omitted for brevity, assuming they are symmetric)
# ... actually PST might be the culprit if I copied them wrong.
# Let's trust they are mirrored correctly for now (using square_mirror).

PAWN_TABLE = [
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
]

# ... re-implement logic to see where 30 comes from

def debug_eval(board):
    score = 0
    # Heuristics
    MOBILITY_BONUS = 5
    CENTER_BONUS = 10
    
    white_mobility = 0
    black_mobility = 0
    
    for square in chess.SQUARES:
        piece = board.piece_at(square)
        if piece is None: continue
        
        attacks = board.attacks(square)
        mob = len(attacks) * MOBILITY_BONUS
        
        if piece.color == chess.WHITE:
            white_mobility += mob
        else:
            black_mobility += mob
            
    print(f"White Mobility: {white_mobility}")
    print(f"Black Mobility: {black_mobility}")
    print(f"Diff: {white_mobility - black_mobility}")
    
    # Check Knights on start
    # White Knights b1, g1. 
    # Attacks from b1: a3, c3, d2. (3 moves)
    # Attacks from g1: f3, h3, e2. (3 moves)
    # Total White Knight Mobility: 6 * 5 = 30?
    
    # Black Knights b8, g8.
    # Attacks from b8: a6, c6, d7. (3 moves)
    # Attacks from g8: f6, h6, e7. (3 moves)
    # Total Black Knight Mobility: 6 * 5 = 30?
    
    # Pawns
    # White e2: d3, f3 (2 moves)
    # Black e7: d6, f6 (2 moves)
    
    # Rooks, Bishops, Queens stuck. Mobility 0.
    # King e1: f1? No blocked. d1? No.
    # King has no moves?
    # board.attacks(e1) -> d1, f1, d2, e2, f2. Friends on all.
    # But attacks returns the set of squares attacked.
    # It includes friendly squares.
    # So mobility metric includes checking "defended" squares.
    return 0

debug_eval(board)
