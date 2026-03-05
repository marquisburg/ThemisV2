import chess

board = chess.Board()
sq = chess.E4

attacks = board.attacks(sq)
print(f"board.attacks(sq) type: {type(attacks)}")
print(f"len(attacks): {len(attacks)}")
try:
    print(f"int(attacks): {int(attacks)}")
except Exception as e:
    print(f"int(attacks) failed: {e}")

attacks_mask = board.attacks_mask(sq) # Does this exist on Board?
# It might be on attacks module or similar.
try:
    print(f"board.attacks_mask(sq): {board.attacks_mask(sq)}")
except AttributeError:
    print("board.attacks_mask does not exist")
    
# Check how to get integer from SquareSet
sq_set = chess.SquareSet(12345)
print(f"SquareSet int: {int(sq_set)}")
