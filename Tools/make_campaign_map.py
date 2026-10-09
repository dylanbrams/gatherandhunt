"""Writes the starter level select map board, Content/BoardSetups/Campaign/Map.dat.

The board holds the map's size, its blocks and the ship at its starting cell;
the level markers come from Campaign.json next to it (see Docs/Design/LevelSelect.md).
Run as an editor commandlet:

    UnrealEditor-Cmd.exe GatherAndHunt.uproject -run=pythonscript
        -script="Tools/make_campaign_map.py" -EnablePlugins=PythonScriptPlugin -nullrhi -unattended

It won't overwrite a map that already exists (it may have been edited in the
board editor since); delete the file first to start over.
"""

import os

import unreal

MAP_FILE = "Campaign/Map"
CELL = 100.0
SIZE = (9, 7, 3)  # cells
SHIP = (0, 3, 0)  # faces East (+X), along the row of the first two levels
BLOCKS = [(3, 5, 0), (5, 1, 0), (7, 5, 0)]

path = os.path.join(unreal.Paths.project_content_dir(), "BoardSetups", MAP_FILE + ".dat")
if os.path.exists(path):
    unreal.log_warning("CAMPAIGNMAP %s already exists; not overwriting it." % path)
else:
    library = unreal.GatherAndHuntLevelEditorLibrary
    # The board file stores its size as the far corner cell's position.
    level = library.create_empty_level_save(
        None, "Level select map", 0, unreal.Vector(*[(cells - 1) * CELL for cells in SIZE]))
    # Act 0 keeps it ahead of the puzzles in the menu's list.
    level.set_editor_property("puzzle_act", 0)
    level.set_editor_property("intro_text", "The level select map. Its level markers are listed in Campaign.json.")

    def add_piece(piece_id, piece_type, cell):
        piece = unreal.PieceSaveData()
        piece.set_editor_property("piece_save_id", piece_id)
        piece.set_editor_property("piece_type", piece_type)
        piece.set_editor_property("position", unreal.Vector(*[axis * CELL for axis in cell]))
        library.upsert_piece_data(level, piece)

    add_piece(1, unreal.PieceTypeEnum.BP_PLAYER_OWNED_PIECE, SHIP)
    for index, block in enumerate(BLOCKS):
        add_piece(2 + index, unreal.PieceTypeEnum.BP_HAZARD, block)

    if library.save_level_save_to_project_file(level, MAP_FILE):
        unreal.log("CAMPAIGNMAP wrote %s" % path)
    else:
        unreal.log_error("CAMPAIGNMAP could not write %s" % path)
