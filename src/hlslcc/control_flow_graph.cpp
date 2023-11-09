#include "debug.h"
#include "control_flow_graph.h"
#include "control_flow_graph_utils.h"
#include "instruction.h"
#include "operand.h"
#include "hlslcc_toolkit.h"
#include <algorithm>

using namespace dxmt::ControlFlow;
using dxmt::ForEachOperand;

const BasicBlock &ControlFlowGraph::Build(const Instruction *firstInstruction,
                                          const Instruction *endInstruction) {
  using std::for_each;

  m_BlockMap.clear();
  m_BlockStorage.clear();

  // Self-registering into m_BlockStorage so it goes out of the scope when
  // ControlFlowGraph does
  BasicBlock *root =
      new BasicBlock(Utils::GetNextNonLabelInstruction(firstInstruction), *this,
                     NULL, endInstruction);

  // Build the reachable set for each block
  bool hadChanges;
  do {
    hadChanges = false;
    for_each(m_BlockStorage.begin(), m_BlockStorage.end(),
             [&](const std::shared_ptr<BasicBlock> &bb) {
               BasicBlock &b = *bb.get();
               if (b.RebuildReachable()) {
                 hadChanges = true;
               }
             });
  } while (hadChanges == true);

  return *root;
}

const BasicBlock *ControlFlowGraph::GetBasicBlockForInstruction(
    const Instruction *instruction) const {
  BasicBlockMap::const_iterator itr =
      m_BlockMap.find(Utils::GetNextNonLabelInstruction(instruction));
  if (itr == m_BlockMap.end())
    return NULL;

  return itr->second;
}

BasicBlock *
ControlFlowGraph::GetBasicBlockForInstruction(const Instruction *instruction) {
  BasicBlockMap::iterator itr =
      m_BlockMap.find(Utils::GetNextNonLabelInstruction(instruction));
  if (itr == m_BlockMap.end())
    return NULL;

  return itr->second;
}

// Generate a basic block. Private constructor, can only be constructed from
// ControlFlowGraph::Build(). Auto-registers itself into ControlFlowGraph
BasicBlock::BasicBlock(const Instruction *psFirst, ControlFlowGraph &graph,
                       const Instruction *psPrecedingBlockHead,
                       const Instruction *endInstruction)
    : m_Graph(graph), m_First(psFirst), m_Last(NULL), m_End(endInstruction) {
  m_UEVar.clear();
  m_VarKill.clear();
  m_Preceding.clear();
  m_Succeeding.clear();
  m_DEDef.clear();
  m_Reachable.clear();

  // Check that we've pruned the labels
  ASSERT(psFirst == Utils::GetNextNonLabelInstruction(psFirst));

  // Insert to block storage, block map and connect to previous block
  m_Graph.m_BlockStorage.push_back(std::shared_ptr<BasicBlock>(this));

  bool didInsert =
      m_Graph.m_BlockMap.insert(std::make_pair(psFirst, this)).second;
  ASSERT(didInsert);

  if (psPrecedingBlockHead != NULL) {
    m_Preceding.insert(psPrecedingBlockHead);
    BasicBlock *prec =
        m_Graph.GetBasicBlockForInstruction(psPrecedingBlockHead);
    ASSERT(prec != 0);
    didInsert = prec->m_Succeeding.insert(psFirst).second;
    ASSERT(didInsert);
  }

  Build();
}

void BasicBlock::Build() {
  const Instruction *inst = m_First;
  while (inst != m_End) {
    // Process sources first
    ForEachOperand(inst, inst + 1, FEO_FLAG_SRC_OPERAND | FEO_FLAG_SUBOPERAND,
                   [this](const Instruction *psInst, const Operand *psOperand,
                          uint32_t ui32OperandType) {
                     if (psOperand->eType != OPERAND_TYPE_TEMP)
                       return;

                     uint32_t tempReg = psOperand->ui32RegisterNumber;
                     uint32_t accessMask = psOperand->GetAccessMask();

                     // Go through each component
                     for (int k = 0; k < 4; k++) {
                       if (!(accessMask & (1 << k)))
                         continue;

                       uint32_t regIdx = tempReg * 4 + k;
                       // Is this idx already in the kill set, meaning that it's
                       // already been re-defined in this basic block? Ignore
                       if (m_VarKill.find(regIdx) != m_VarKill.end())
                         continue;

                       // Add to UEVars set. Doesn't matter if it's already
                       // there.
                       m_UEVar.insert(regIdx);
                     }
                     return;
                   });

    // Then the destination operands
    ForEachOperand(inst, inst + 1, FEO_FLAG_DEST_OPERAND,
                   [this](const Instruction *psInst, const Operand *psOperand,
                          uint32_t ui32OperandType) {
                     if (psOperand->eType != OPERAND_TYPE_TEMP)
                       return;

                     uint32_t tempReg = psOperand->ui32RegisterNumber;
                     uint32_t accessMask = psOperand->GetAccessMask();

                     // Go through each component
                     for (int k = 0; k < 4; k++) {
                       if (!(accessMask & (1 << k)))
                         continue;

                       uint32_t regIdx = tempReg * 4 + k;

                       // Add to kill set. Dupes are fine, this is a set.
                       m_VarKill.insert(regIdx);
                       // Also into the downward definitions. Overwrite the
                       // previous definition in this basic block, if any
                       Definition d(psInst, psOperand);
                       m_DEDef[regIdx].clear();
                       m_DEDef[regIdx].insert(d);
                     }
                     return;
                   });

    // Check for flow control instructions
    bool blockDone = false;
    switch (inst->eOpcode) {
    default:
      break;
    case OPCODE_RET:
      // Continue processing, in the case of unreachable code we still need to
      // translate it properly (case 1160309) blockDone = true;
      break;
    case OPCODE_RETC:
      // Basic block is done, start a next one.
      // There REALLY should be no existing blocks for this one
      ASSERT(m_Graph.GetBasicBlockForInstruction(
                 Utils::GetNextNonLabelInstruction(inst + 1)) == NULL);
      AddChildBasicBlock(Utils::GetNextNonLabelInstruction(inst + 1));
      blockDone = true;
      break;
    case OPCODE_LOOP:
    case OPCODE_CASE:
    case OPCODE_ENDIF:
    case OPCODE_ENDSWITCH:
      // Not a flow control branch, but need to start a new block anyway.
      AddChildBasicBlock(Utils::GetNextNonLabelInstruction(inst + 1));
      blockDone = true;
      break;

    // Branches
    case OPCODE_IF:
    case OPCODE_BREAKC:
    case OPCODE_CONTINUEC: {
      const Instruction *jumpPoint = Utils::GetJumpPoint(inst);
      ASSERT(jumpPoint != NULL);

      // The control branches to the next instruction or jumps to jumpPoint
      AddChildBasicBlock(Utils::GetNextNonLabelInstruction(inst + 1));
      AddChildBasicBlock(jumpPoint);

      blockDone = true;
      break;
    }
    case OPCODE_SWITCH: {
      bool sawEndSwitch = false;
      bool needConnectToParent = false;
      const Instruction *jumpPoint =
          Utils::GetJumpPoint(inst, &sawEndSwitch, &needConnectToParent);
      ASSERT(jumpPoint != NULL);

      while (1) {
        if (!sawEndSwitch || needConnectToParent)
          AddChildBasicBlock(jumpPoint);

        if (sawEndSwitch)
          break;

        // The -1 is a bit of a hack: we always scroll past all labels so rewind
        // to the last one so we'll know to search for the next label
        ASSERT((jumpPoint - 1)->eOpcode == OPCODE_CASE ||
               (jumpPoint - 1)->eOpcode == OPCODE_DEFAULT);
        jumpPoint = Utils::GetJumpPoint(jumpPoint - 1, &sawEndSwitch,
                                        &needConnectToParent);
        ASSERT(jumpPoint != NULL);
      }
      blockDone = true;
      break;
    }

    // Non-conditional jumps
    case OPCODE_BREAK:
    case OPCODE_ELSE:
    case OPCODE_CONTINUE:
    case OPCODE_ENDLOOP: {
      const Instruction *jumpPoint = Utils::GetJumpPoint(inst);
      ASSERT(jumpPoint != NULL);

      AddChildBasicBlock(jumpPoint);

      blockDone = true;
      break;
    }
    }

    if (blockDone)
      break;

    inst++;
  }
  // In initial building phase, just make m_Reachable equal to m_DEDef
  m_Reachable = m_DEDef;

  // Tag the end of the basic block
  m_Last = std::max(m_First, std::min(inst, m_End - 1));
  //  printf("Basic Block %d -> %d\n", (int)m_First->id, (int)m_Last->id);
}

BasicBlock *BasicBlock::AddChildBasicBlock(const Instruction *psFirst) {
  // First see if this already exists
  BasicBlock *b = m_Graph.GetBasicBlockForInstruction(psFirst);
  if (b) {
    // Just add dependency and we're done
    b->m_Preceding.insert(m_First);
    m_Succeeding.insert(psFirst);
    return b;
  }
  // Otherwise create one. Self-registering and self-connecting
  return new BasicBlock(psFirst, m_Graph, m_First, m_End);
}

bool BasicBlock::RebuildReachable() {
  // Building the Reachable set is an iterative process, where each block gets
  // rebuilt until nothing changes. Formula: reachable = this.DEDef union ( each
  // preceding.Reachable() minus this.VarKill())

  ReachableVariables newReachable = m_DEDef;
  bool hasChanges = false;

  // Loop each predecessor
  std::for_each(
      Preceding().begin(), Preceding().end(), [&](const Instruction *instr) {
        const BasicBlock *prec = m_Graph.GetBasicBlockForInstruction(instr);
        const ReachableVariables &precReachable = prec->Reachable();

        // Loop each variable*component
        std::for_each(
            precReachable.begin(), precReachable.end(),
            [&](const std::pair<
                uint32_t, BasicBlock::ReachableDefinitionsPerVariable> &itr2) {
              uint32_t regIdx = itr2.first;
              const BasicBlock::ReachableDefinitionsPerVariable &defs =
                  itr2.second;

              // Already killed in this block?
              if (VarKill().find(regIdx) != VarKill().end())
                return;

              // Only do comparisons against current definitions if we've yet to
              // find any changes
              BasicBlock::ReachableDefinitionsPerVariable *currReachablePerVar =
                  0;
              if (!hasChanges)
                currReachablePerVar = &m_Reachable[regIdx];

              BasicBlock::ReachableDefinitionsPerVariable &newReachablePerVar =
                  newReachable[regIdx];

              // Loop each definition
              std::for_each(defs.begin(), defs.end(),
                            [&](const BasicBlock::Definition &d) {
                              if (!hasChanges) {
                                // Check if already there
                                if (currReachablePerVar->find(d) ==
                                    currReachablePerVar->end())
                                  hasChanges = true;
                              }
                              newReachablePerVar.insert(d);
                            }); // definition
            });                 // variable*component
      });                       // predecessor

  if (hasChanges) {
    std::swap(m_Reachable, newReachable);
  }

  return hasChanges;
}

void BasicBlock::RVarUnion(ReachableVariables &a, const ReachableVariables &b) {
  std::for_each(
      b.begin(), b.end(),
      [&a](
          const std::pair<uint32_t, ReachableDefinitionsPerVariable> &rpvPair) {
        uint32_t regIdx = rpvPair.first;
        const ReachableDefinitionsPerVariable &rpv = rpvPair.second;
        // No previous definitions for this variable?
        auto aRPVItr = a.find(regIdx);
        if (aRPVItr == a.end()) {
          // Just set the definitions and continue
          a[regIdx] = rpv;
          return;
        }
        ReachableDefinitionsPerVariable &aRPV = aRPVItr->second;
        aRPV.insert(rpv.begin(), rpv.end());
      });
}