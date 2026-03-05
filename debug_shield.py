import chess

board = chess.Board()
white_pawns = board.pieces(chess.PAWN, chess.WHITE)
black_pawns = board.pieces(chess.PAWN, chess.BLACK)
white_pawns_bb = int(white_pawns)
black_pawns_bb = int(black_pawns)

score = 0
KING_SHIELD_BONUS = 15

# White King Safety
k_file = 4 # e
for f in range(3, 6):
    file_mask = chess.BB_FILES[f]
    shield_pawns = white_pawns_bb & file_mask & (chess.BB_RANKS[1] | chess.BB_RANKS[2])
    if shield_pawns:
        print(f"White Shield Found at file {f}")
        score += KING_SHIELD_BONUS

print(f"Score after White: {score}")

# Black King Safety
k_file = 4 # e
for f in range(3, 6):
    file_mask = chess.BB_FILES[f]
    shield_pawns = black_pawns_bb & file_mask & (chess.BB_RANKS[6] | chess.BB_RANKS[7])
    if shield_pawns:
         print(f"Black Shield Found at file {f}")
         score -= KING_SHIELD_BONUS

print(f"Final Score: {score}")
