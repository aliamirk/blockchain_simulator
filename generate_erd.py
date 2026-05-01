"""
Generate a Chen Notation ERD for the Blockchain Simulator Oracle Database.
Uses matplotlib to draw entities (rectangles), relationships (diamonds),
and attributes (ovals) in proper Chen notation.
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch
import numpy as np

fig, ax = plt.subplots(1, 1, figsize=(28, 36))
ax.set_xlim(-2, 28)
ax.set_ylim(-2, 36)
ax.set_aspect('equal')
ax.axis('off')
fig.patch.set_facecolor('white')

# Colors
ENTITY_COLOR = '#4A90D9'
ENTITY_TEXT = 'white'
REL_COLOR = '#E74C3C'
REL_TEXT = 'white'
ATTR_COLOR = '#F5F5F5'
ATTR_BORDER = '#333333'
PK_COLOR = '#FFF3CD'
PK_BORDER = '#856404'
FK_COLOR = '#D4EDDA'
FK_BORDER = '#155724'

def draw_entity(ax, x, y, name, w=3.2, h=1.0):
    """Draw a rectangle for an entity."""
    rect = FancyBboxPatch((x - w/2, y - h/2), w, h,
                          boxstyle="round,pad=0.05",
                          facecolor=ENTITY_COLOR, edgecolor='#2C3E50', linewidth=2.5)
    ax.add_patch(rect)
    ax.text(x, y, name, ha='center', va='center',
            fontsize=10, fontweight='bold', color=ENTITY_TEXT)
    return (x, y)

def draw_relationship(ax, x, y, name, w=2.8, h=0.9):
    """Draw a diamond for a relationship."""
    diamond = plt.Polygon([
        (x, y + h/2),
        (x + w/2, y),
        (x, y - h/2),
        (x - w/2, y)
    ], closed=True, facecolor=REL_COLOR, edgecolor='#922B21', linewidth=2)
    ax.add_patch(diamond)
    ax.text(x, y, name, ha='center', va='center',
            fontsize=8, fontweight='bold', color=REL_TEXT)
    return (x, y)

def draw_attribute(ax, x, y, name, is_pk=False, is_fk=False, is_multivalued=False):
    """Draw an oval for an attribute. PK is underlined, FK is dashed border."""
    if is_pk:
        fc, ec = PK_COLOR, PK_BORDER
    elif is_fk:
        fc, ec = FK_COLOR, FK_BORDER
    else:
        fc, ec = ATTR_COLOR, ATTR_BORDER

    ls = '--' if is_fk else '-'
    lw = 1.5 if is_pk else 1.0

    ellipse = mpatches.Ellipse((x, y), 1.8, 0.55,
                               facecolor=fc, edgecolor=ec,
                               linewidth=lw, linestyle=ls)
    ax.add_patch(ellipse)

    if is_pk:
        ax.text(x, y, name, ha='center', va='center', fontsize=7,
                fontweight='bold', style='normal',
                bbox=dict(boxstyle='round,pad=0.01', facecolor='none', edgecolor='none'))
        # Underline for PK
        text_width = len(name) * 0.065
        ax.plot([x - text_width, x + text_width], [y - 0.12, y - 0.12],
                color=PK_BORDER, linewidth=1.5)
    else:
        ax.text(x, y, name, ha='center', va='center', fontsize=7)

    return (x, y)

def draw_line(ax, p1, p2, style='-', color='#555555', lw=1.0):
    """Draw a line between two points."""
    ax.plot([p1[0], p2[0]], [p1[1], p2[1]], style, color=color, linewidth=lw)

def draw_cardinality(ax, x, y, text):
    """Draw cardinality label."""
    ax.text(x, y, text, ha='center', va='center', fontsize=8,
            fontweight='bold', color='#333333',
            bbox=dict(boxstyle='round,pad=0.15', facecolor='#FFFFEE', edgecolor='#999999', linewidth=0.5))

# ============================================================
# ENTITY POSITIONS
# ============================================================

# Row 1 (top): BLOCKCHAIN_META, MINERS, TRANSACTION_TYPES
e_meta = draw_entity(ax, 5, 33, 'BLOCKCHAIN_META', w=3.6)
e_miners = draw_entity(ax, 14, 33, 'MINERS')
e_types = draw_entity(ax, 23, 33, 'TRANSACTION_TYPES', w=3.8)

# Row 2: BLOCKS
e_blocks = draw_entity(ax, 9.5, 25, 'BLOCKS')

# Row 3: TRANSACTIONS, PENDING_TRANSACTIONS
e_trans = draw_entity(ax, 9.5, 17, 'TRANSACTIONS', w=3.4)
e_pending = draw_entity(ax, 21, 17, 'PENDING_TRANSACTIONS', w=4.2)

# Row 4 (bottom): ACCOUNTS, AUDIT_LOG, CHAIN_STATISTICS
e_accounts = draw_entity(ax, 3, 9, 'ACCOUNTS')
e_audit = draw_entity(ax, 13, 9, 'AUDIT_LOG')
e_stats = draw_entity(ax, 23, 9, 'CHAIN_STATISTICS', w=3.8)

# ============================================================
# RELATIONSHIPS (Diamonds)
# ============================================================

r_has_blocks = draw_relationship(ax, 7, 29, 'has')
r_mines = draw_relationship(ax, 12, 29, 'mines')
r_contains = draw_relationship(ax, 9.5, 21, 'contains')
r_categorizes = draw_relationship(ax, 16, 21, 'categorizes')
r_cat_pending = draw_relationship(ax, 22, 25, 'categorizes')
r_tracked = draw_relationship(ax, 14, 5, 'tracked_by')

# ============================================================
# RELATIONSHIP LINES
# ============================================================

# BLOCKCHAIN_META --has--> BLOCKS
draw_line(ax, (5, 32.5), (7, 29.45), color='#2C3E50', lw=1.5)
draw_line(ax, (7, 28.55), (9.5, 25.5), color='#2C3E50', lw=1.5)
draw_cardinality(ax, 5.5, 31, '1')
draw_cardinality(ax, 8.5, 26.8, 'N')

# MINERS --mines--> BLOCKS
draw_line(ax, (14, 32.5), (12, 29.45), color='#2C3E50', lw=1.5)
draw_line(ax, (12, 28.55), (9.5, 25.5), color='#2C3E50', lw=1.5)
draw_cardinality(ax, 13.3, 31, '1')
draw_cardinality(ax, 10.5, 26.8, 'N')

# BLOCKS --contains--> TRANSACTIONS
draw_line(ax, (9.5, 24.5), (9.5, 21.45), color='#2C3E50', lw=1.5)
draw_line(ax, (9.5, 20.55), (9.5, 17.5), color='#2C3E50', lw=1.5)
draw_cardinality(ax, 10.1, 23, '1')
draw_cardinality(ax, 10.1, 19, 'N')

# TRANSACTION_TYPES --categorizes--> TRANSACTIONS
draw_line(ax, (23, 32.5), (23, 30))
draw_line(ax, (23, 30), (16, 21.45), color='#2C3E50', lw=1.5)
draw_line(ax, (16, 20.55), (12, 17.5), color='#2C3E50', lw=1.5)
draw_cardinality(ax, 20, 26, '1')
draw_cardinality(ax, 13.5, 18.8, 'N')

# TRANSACTION_TYPES --categorizes--> PENDING_TRANSACTIONS
draw_line(ax, (23, 32.5), (22, 25.45), color='#2C3E50', lw=1.5)
draw_line(ax, (22, 24.55), (21, 17.5), color='#2C3E50', lw=1.5)
draw_cardinality(ax, 22.8, 29, '1')
draw_cardinality(ax, 21.3, 21, 'N')

# BLOCKCHAIN_META --tracked_by--> CHAIN_STATISTICS
draw_line(ax, (5, 32.5), (3, 12))
draw_line(ax, (3, 12), (14, 5.45), color='#2C3E50', lw=1.5)
draw_line(ax, (14, 4.55), (23, 9.0 - 0.5), color='#2C3E50', lw=1.5)
draw_cardinality(ax, 8, 6, '1')
draw_cardinality(ax, 19, 6.5, '1')

# ============================================================
# ATTRIBUTES
# ============================================================

# --- BLOCKCHAIN_META attributes ---
attrs_meta = [
    ('CHAIN_ID', True, False), ('NAME', False, False),
    ('DIFFICULTY', False, False), ('NEXT_TX_ID', False, False),
    ('CREATED_AT', False, False)
]
meta_ax = 1.5
for i, (name, pk, fk) in enumerate(attrs_meta):
    ay = 35.5 - i * 0.7
    pos = draw_attribute(ax, meta_ax, ay, name, is_pk=pk, is_fk=fk)
    draw_line(ax, pos, e_meta, color='#888888', lw=0.8)

# --- MINERS attributes ---
attrs_miners = [
    ('MINER_ID', True, False), ('NAME', False, False),
    ('REGISTERED_AT', False, False)
]
for i, (name, pk, fk) in enumerate(attrs_miners):
    ay = 35.5 - i * 0.7
    pos = draw_attribute(ax, 14, ay, name, is_pk=pk, is_fk=fk)
    draw_line(ax, pos, e_miners, color='#888888', lw=0.8)

# --- TRANSACTION_TYPES attributes ---
attrs_types = [
    ('TYPE_ID', True, False), ('TYPE_NAME', False, False),
    ('DESCRIPTION', False, False)
]
for i, (name, pk, fk) in enumerate(attrs_types):
    ay = 35.5 - i * 0.7
    pos = draw_attribute(ax, 26, ay, name, is_pk=pk, is_fk=fk)
    draw_line(ax, pos, e_types, color='#888888', lw=0.8)

# --- BLOCKS attributes ---
attrs_blocks = [
    ('BLOCK_ID', True, False), ('CHAIN_ID', False, True),
    ('MINER_ID', False, True), ('BLOCK_INDEX', False, False),
    ('BLOCK_TIMESTAMP', False, False), ('NONCE', False, False),
    ('HASH', False, False), ('PREV_HASH', False, False)
]
bx_left = 4.5
for i, (name, pk, fk) in enumerate(attrs_blocks):
    if i < 4:
        pos = draw_attribute(ax, bx_left, 27 - i * 0.7, name, is_pk=pk, is_fk=fk)
    else:
        pos = draw_attribute(ax, bx_left + 9, 27 - (i-4) * 0.7, name, is_pk=pk, is_fk=fk)
    draw_line(ax, pos, e_blocks, color='#888888', lw=0.8)

# --- TRANSACTIONS attributes ---
attrs_trans = [
    ('TRANSACTION_ID', True, False), ('BLOCK_ID', False, True),
    ('TYPE_ID', False, True), ('TX_ID', False, False),
    ('SENDER', False, False), ('RECEIVER', False, False),
    ('AMOUNT', False, False), ('METADATA', False, False),
    ('SIGNATURE', False, False)
]
for i, (name, pk, fk) in enumerate(attrs_trans):
    if i < 5:
        pos = draw_attribute(ax, 5, 19 - i * 0.7, name, is_pk=pk, is_fk=fk)
    else:
        pos = draw_attribute(ax, 14, 19 - (i-5) * 0.7, name, is_pk=pk, is_fk=fk)
    draw_line(ax, pos, e_trans, color='#888888', lw=0.8)

# --- PENDING_TRANSACTIONS attributes ---
attrs_pending = [
    ('PENDING_TX_ID', True, False), ('TYPE_ID', False, True),
    ('SENDER', False, False), ('RECEIVER', False, False),
    ('AMOUNT', False, False), ('METADATA', False, False),
    ('SIGNATURE', False, False)
]
for i, (name, pk, fk) in enumerate(attrs_pending):
    if i < 4:
        pos = draw_attribute(ax, 18, 19 - i * 0.7, name, is_pk=pk, is_fk=fk)
    else:
        pos = draw_attribute(ax, 25, 19 - (i-4) * 0.7, name, is_pk=pk, is_fk=fk)
    draw_line(ax, pos, e_pending, color='#888888', lw=0.8)

# --- ACCOUNTS attributes ---
attrs_accounts = [
    ('ACCOUNT_ID', True, False), ('NAME', False, False),
    ('BALANCE', False, False), ('CREATED_AT', False, False)
]
for i, (name, pk, fk) in enumerate(attrs_accounts):
    pos = draw_attribute(ax, 0.5, 11 - i * 0.7, name, is_pk=pk, is_fk=fk)
    draw_line(ax, pos, e_accounts, color='#888888', lw=0.8)

# --- AUDIT_LOG attributes ---
attrs_audit = [
    ('LOG_ID', True, False), ('OPERATION', False, False),
    ('CHAIN_NAME', False, False), ('DETAILS', False, False),
    ('LOG_TIMESTAMP', False, False)
]
for i, (name, pk, fk) in enumerate(attrs_audit):
    pos = draw_attribute(ax, 13, 11.5 - i * 0.7, name, is_pk=pk, is_fk=fk)
    draw_line(ax, pos, e_audit, color='#888888', lw=0.8)

# --- CHAIN_STATISTICS attributes ---
attrs_stats = [
    ('STAT_ID', True, False), ('CHAIN_ID', False, True),
    ('TOTAL_BLOCKS', False, False), ('TOTAL_TRANSACTIONS', False, False),
    ('TOTAL_PENDING', False, False), ('LAST_UPDATED', False, False)
]
for i, (name, pk, fk) in enumerate(attrs_stats):
    pos = draw_attribute(ax, 23, 7.5 - i * 0.7, name, is_pk=pk, is_fk=fk)
    draw_line(ax, pos, e_stats, color='#888888', lw=0.8)

# ============================================================
# LEGEND
# ============================================================
legend_x = 0.5
legend_y = 2.5

ax.text(legend_x, legend_y + 1.5, 'LEGEND (Chen Notation)', fontsize=10, fontweight='bold')

# Entity
rect = FancyBboxPatch((legend_x, legend_y + 0.6), 1.5, 0.6,
                      boxstyle="round,pad=0.05",
                      facecolor=ENTITY_COLOR, edgecolor='#2C3E50', linewidth=1.5)
ax.add_patch(rect)
ax.text(legend_x + 0.75, legend_y + 0.9, 'Entity', ha='center', va='center',
        fontsize=8, color='white', fontweight='bold')
ax.text(legend_x + 2, legend_y + 0.9, '= Table (Rectangle)', fontsize=8, va='center')

# Relationship
diamond = plt.Polygon([
    (legend_x + 0.75, legend_y + 0.3),
    (legend_x + 1.5, legend_y),
    (legend_x + 0.75, legend_y - 0.3),
    (legend_x, legend_y)
], closed=True, facecolor=REL_COLOR, edgecolor='#922B21', linewidth=1.5)
ax.add_patch(diamond)
ax.text(legend_x + 2, legend_y, '= Relationship (Diamond)', fontsize=8, va='center')

# PK Attribute
ellipse = mpatches.Ellipse((legend_x + 0.75, legend_y - 0.8), 1.4, 0.45,
                           facecolor=PK_COLOR, edgecolor=PK_BORDER, linewidth=1.5)
ax.add_patch(ellipse)
ax.text(legend_x + 0.75, legend_y - 0.8, 'PK', ha='center', va='center', fontsize=7, fontweight='bold')
ax.plot([legend_x + 0.55, legend_x + 0.95], [legend_y - 0.92, legend_y - 0.92], color=PK_BORDER, linewidth=1.5)
ax.text(legend_x + 2, legend_y - 0.8, '= Primary Key (Underlined, Yellow)', fontsize=8, va='center')

# FK Attribute
ellipse2 = mpatches.Ellipse((legend_x + 0.75, legend_y - 1.4), 1.4, 0.45,
                            facecolor=FK_COLOR, edgecolor=FK_BORDER, linewidth=1, linestyle='--')
ax.add_patch(ellipse2)
ax.text(legend_x + 0.75, legend_y - 1.4, 'FK', ha='center', va='center', fontsize=7)
ax.text(legend_x + 2, legend_y - 1.4, '= Foreign Key (Dashed, Green)', fontsize=8, va='center')

# Regular Attribute
ellipse3 = mpatches.Ellipse((legend_x + 0.75, legend_y - 2.0), 1.4, 0.45,
                            facecolor=ATTR_COLOR, edgecolor=ATTR_BORDER, linewidth=1)
ax.add_patch(ellipse3)
ax.text(legend_x + 0.75, legend_y - 2.0, 'Attr', ha='center', va='center', fontsize=7)
ax.text(legend_x + 2, legend_y - 2.0, '= Regular Attribute (Oval)', fontsize=8, va='center')

# Cardinality
ax.text(legend_x, legend_y - 2.7, '1, N = Cardinality (1:One, N:Many)', fontsize=8)

# Title
ax.text(14, 36.5, 'Blockchain Simulator — Chen Notation ERD',
        ha='center', va='center', fontsize=16, fontweight='bold', color='#2C3E50')
ax.text(14, 35.8, '9 Tables | 6 Relationships | Oracle Database Schema (3NF)',
        ha='center', va='center', fontsize=11, color='#555555')

plt.tight_layout()
plt.savefig('erd_chen_notation.png', dpi=150, bbox_inches='tight',
            facecolor='white', edgecolor='none')
plt.close()
print("ERD saved as erd_chen_notation.png")
