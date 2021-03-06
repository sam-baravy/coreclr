// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

// Inlining Policies
//
// This file contains class definitions for various inlining
// policies used by the jit.
//
// -- CLASSES --
//
// LegalPolicy          - partial class providing common legality checks
// LegacyPolicy         - policy that provides legacy inline behavior
// EnhancedLegacyPolicy - legacy variant with some enhancements
// DiscretionaryPolicy  - legacy variant with uniform size policy
// ModelPolicy          - policy based on statistical modelling
//
// These experimental policies are available only in
// DEBUG or release+INLINE_DATA builds of the jit.
//
// RandomPolicy         - randomized inlining
// FullPolicy           - inlines everything up to size and depth limits
// SizePolicy           - tries not to increase method sizes
//
// The default policy in use is the EnhancedLegacyPolicy.

#ifndef _INLINE_POLICY_H_
#define _INLINE_POLICY_H_

#include "jit.h"
#include "inline.h"

// LegalPolicy is a partial policy that encapsulates the common
// legality and ability checks the inliner must make.
//
// Generally speaking, the legal policy expects the inlining attempt
// to fail fast when a fatal or equivalent observation is made. So
// once an observation causes failure, no more observations are
// expected. However for the prejit scan case (where the jit is not
// actually inlining, but is assessing a method's general
// inlinability) the legal policy allows multiple failing
// observations provided they have the same impact. Only the first
// observation that puts the policy into a failing state is
// remembered. Transitions from failing states to candidate or success
// states are not allowed.

class LegalPolicy : public InlinePolicy
{

public:
    // Constructor
    LegalPolicy(bool isPrejitRoot) : InlinePolicy(isPrejitRoot)
    {
        // empty
    }

    // Handle an observation that must cause inlining to fail.
    void NoteFatal(InlineObservation obs) override;

protected:
    // Helper methods
    void NoteInternal(InlineObservation obs);
    void SetCandidate(InlineObservation obs);
    void SetFailure(InlineObservation obs);
    void SetNever(InlineObservation obs);
};

// Forward declaration for the state machine class used by the
// LegacyPolicy

class CodeSeqSM;

// LegacyPolicy implements the inlining policy used by the jit in its
// initial release.

class LegacyPolicy : public LegalPolicy
{
public:
    // Construct a LegacyPolicy
    LegacyPolicy(Compiler* compiler, bool isPrejitRoot)
        : LegalPolicy(isPrejitRoot)
        , m_RootCompiler(compiler)
        , m_StateMachine(nullptr)
        , m_Multiplier(0.0)
        , m_CodeSize(0)
        , m_CallsiteFrequency(InlineCallsiteFrequency::UNUSED)
        , m_InstructionCount(0)
        , m_LoadStoreCount(0)
        , m_ArgFeedsConstantTest(0)
        , m_ArgFeedsRangeCheck(0)
        , m_ConstantArgFeedsConstantTest(0)
        , m_CalleeNativeSizeEstimate(0)
        , m_CallsiteNativeSizeEstimate(0)
        , m_IsForceInline(false)
        , m_IsForceInlineKnown(false)
        , m_IsInstanceCtor(false)
        , m_IsFromPromotableValueClass(false)
        , m_HasSimd(false)
        , m_LooksLikeWrapperMethod(false)
        , m_MethodIsMostlyLoadStore(false)
    {
        // empty
    }

    // Policy observations
    void NoteSuccess() override;
    void NoteBool(InlineObservation obs, bool value) override;
    void NoteInt(InlineObservation obs, int value) override;

    // Policy determinations
    void DetermineProfitability(CORINFO_METHOD_INFO* methodInfo) override;

    // Policy policies
    bool PropagateNeverToRuntime() const override
    {
        return true;
    }
    bool IsLegacyPolicy() const override
    {
        return true;
    }

    // Policy estimates
    int CodeSizeEstimate() override;

#if defined(DEBUG) || defined(INLINE_DATA)

    const char* GetName() const override
    {
        return "LegacyPolicy";
    }

#endif // (DEBUG) || defined(INLINE_DATA)

protected:
    // Constants
    enum
    {
        MAX_BASIC_BLOCKS = 5,
        SIZE_SCALE       = 10
    };

    // Helper methods
    double DetermineMultiplier();
    int    DetermineNativeSizeEstimate();
    int DetermineCallsiteNativeSizeEstimate(CORINFO_METHOD_INFO* methodInfo);

    // Data members
    Compiler*               m_RootCompiler; // root compiler instance
    CodeSeqSM*              m_StateMachine;
    double                  m_Multiplier;
    unsigned                m_CodeSize;
    InlineCallsiteFrequency m_CallsiteFrequency;
    unsigned                m_InstructionCount;
    unsigned                m_LoadStoreCount;
    unsigned                m_ArgFeedsConstantTest;
    unsigned                m_ArgFeedsRangeCheck;
    unsigned                m_ConstantArgFeedsConstantTest;
    int                     m_CalleeNativeSizeEstimate;
    int                     m_CallsiteNativeSizeEstimate;
    bool                    m_IsForceInline : 1;
    bool                    m_IsForceInlineKnown : 1;
    bool                    m_IsInstanceCtor : 1;
    bool                    m_IsFromPromotableValueClass : 1;
    bool                    m_HasSimd : 1;
    bool                    m_LooksLikeWrapperMethod : 1;
    bool                    m_MethodIsMostlyLoadStore : 1;
};

// EnhancedLegacyPolicy extends the legacy policy by rejecting
// inlining of methods that never return because they throw.

class EnhancedLegacyPolicy : public LegacyPolicy
{
public:
    EnhancedLegacyPolicy(Compiler* compiler, bool isPrejitRoot)
        : LegacyPolicy(compiler, isPrejitRoot), m_IsNoReturn(false), m_IsNoReturnKnown(false)
    {
        // empty
    }

    // Policy observations
    void NoteBool(InlineObservation obs, bool value) override;
    void NoteInt(InlineObservation obs, int value) override;

    // Policy policies
    bool PropagateNeverToRuntime() const override;
    bool IsLegacyPolicy() const override
    {
        return false;
    }

protected:
    // Data members
    bool m_IsNoReturn : 1;
    bool m_IsNoReturnKnown : 1;
};

#ifdef DEBUG

// RandomPolicy implements a policy that inlines at random.
// It is mostly useful for stress testing.

class RandomPolicy : public LegalPolicy
{
public:
    // Construct a RandomPolicy
    RandomPolicy(Compiler* compiler, bool isPrejitRoot, unsigned seed);

    // Policy observations
    void NoteSuccess() override;
    void NoteBool(InlineObservation obs, bool value) override;
    void NoteInt(InlineObservation obs, int value) override;

    // Policy determinations
    void DetermineProfitability(CORINFO_METHOD_INFO* methodInfo) override;

    // Policy policies
    bool PropagateNeverToRuntime() const override
    {
        return true;
    }
    bool IsLegacyPolicy() const override
    {
        return false;
    }

    // Policy estimates
    int CodeSizeEstimate() override
    {
        return 0;
    }

    const char* GetName() const override
    {
        return "RandomPolicy";
    }

private:
    // Data members
    Compiler*  m_RootCompiler;
    CLRRandom* m_Random;
    unsigned   m_CodeSize;
    bool       m_IsForceInline : 1;
    bool       m_IsForceInlineKnown : 1;
};

#endif // DEBUG

// DiscretionaryPolicy is a variant of the legacy policy.  It differs
// in that there is no ALWAYS_INLINE class, there is no IL size limit,
// it does not try and maintain legacy compatabilty, and in prejit mode,
// discretionary failures do not set the "NEVER" inline bit.
//
// It is useful for gathering data about inline costs.

class DiscretionaryPolicy : public LegacyPolicy
{
public:
    // Construct a DiscretionaryPolicy
    DiscretionaryPolicy(Compiler* compiler, bool isPrejitRoot);

    // Policy observations
    void NoteBool(InlineObservation obs, bool value) override;
    void NoteInt(InlineObservation obs, int value) override;

    // Policy policies
    bool PropagateNeverToRuntime() const override;
    bool IsLegacyPolicy() const override
    {
        return false;
    }

    // Policy determinations
    void DetermineProfitability(CORINFO_METHOD_INFO* methodInfo) override;

    // Policy estimates
    int CodeSizeEstimate() override;

#if defined(DEBUG) || defined(INLINE_DATA)

    // Externalize data
    void DumpData(FILE* file) const override;
    void DumpSchema(FILE* file) const override;

    // Miscellaneous
    const char* GetName() const override
    {
        return "DiscretionaryPolicy";
    }

#endif // defined(DEBUG) || defined(INLINE_DATA)

protected:
    void ComputeOpcodeBin(OPCODE opcode);
    void EstimateCodeSize();
    void EstimatePerformanceImpact();
    void MethodInfoObservations(CORINFO_METHOD_INFO* methodInfo);
    enum
    {
        MAX_ARGS = 6
    };

    unsigned    m_Depth;
    unsigned    m_BlockCount;
    unsigned    m_Maxstack;
    unsigned    m_ArgCount;
    CorInfoType m_ArgType[MAX_ARGS];
    size_t      m_ArgSize[MAX_ARGS];
    unsigned    m_LocalCount;
    CorInfoType m_ReturnType;
    size_t      m_ReturnSize;
    unsigned    m_ArgAccessCount;
    unsigned    m_LocalAccessCount;
    unsigned    m_IntConstantCount;
    unsigned    m_FloatConstantCount;
    unsigned    m_IntLoadCount;
    unsigned    m_FloatLoadCount;
    unsigned    m_IntStoreCount;
    unsigned    m_FloatStoreCount;
    unsigned    m_SimpleMathCount;
    unsigned    m_ComplexMathCount;
    unsigned    m_OverflowMathCount;
    unsigned    m_IntArrayLoadCount;
    unsigned    m_FloatArrayLoadCount;
    unsigned    m_RefArrayLoadCount;
    unsigned    m_StructArrayLoadCount;
    unsigned    m_IntArrayStoreCount;
    unsigned    m_FloatArrayStoreCount;
    unsigned    m_RefArrayStoreCount;
    unsigned    m_StructArrayStoreCount;
    unsigned    m_StructOperationCount;
    unsigned    m_ObjectModelCount;
    unsigned    m_FieldLoadCount;
    unsigned    m_FieldStoreCount;
    unsigned    m_StaticFieldLoadCount;
    unsigned    m_StaticFieldStoreCount;
    unsigned    m_LoadAddressCount;
    unsigned    m_ThrowCount;
    unsigned    m_ReturnCount;
    unsigned    m_CallCount;
    unsigned    m_CallSiteWeight;
    int         m_ModelCodeSizeEstimate;
    int         m_PerCallInstructionEstimate;
    bool        m_IsClassCtor;
    bool        m_IsSameThis;
    bool        m_CallerHasNewArray;
    bool        m_CallerHasNewObj;
};

// ModelPolicy is an experimental policy that uses the results
// of data modelling to make estimates.

class ModelPolicy : public DiscretionaryPolicy
{
public:
    // Construct a ModelPolicy
    ModelPolicy(Compiler* compiler, bool isPrejitRoot);

    // Policy observations
    void NoteInt(InlineObservation obs, int value) override;

    // Policy determinations
    void DetermineProfitability(CORINFO_METHOD_INFO* methodInfo) override;

    // Policy policies
    bool PropagateNeverToRuntime() const override
    {
        return true;
    }

#if defined(DEBUG) || defined(INLINE_DATA)

    // Miscellaneous
    const char* GetName() const override
    {
        return "ModelPolicy";
    }

#endif // defined(DEBUG) || defined(INLINE_DATA)
};

#if defined(DEBUG) || defined(INLINE_DATA)

// FullPolicy is an experimental policy that will always inline if
// possible, subject to externally settable depth and size limits.
//
// It's useful for unconvering the full set of possible inlines for
// methods.

class FullPolicy : public DiscretionaryPolicy
{
public:
    // Construct a FullPolicy
    FullPolicy(Compiler* compiler, bool isPrejitRoot);

    // Policy determinations
    void DetermineProfitability(CORINFO_METHOD_INFO* methodInfo) override;

    // Miscellaneous
    const char* GetName() const override
    {
        return "FullPolicy";
    }
};

// SizePolicy is an experimental policy that will inline as much
// as possible without increasing the (estimated) method size.
//
// It may be useful down the road as a policy to use for methods
// that are rarely executed (eg class constructors).

class SizePolicy : public DiscretionaryPolicy
{
public:
    // Construct a SizePolicy
    SizePolicy(Compiler* compiler, bool isPrejitRoot);

    // Policy determinations
    void DetermineProfitability(CORINFO_METHOD_INFO* methodInfo) override;

    // Miscellaneous
    const char* GetName() const override
    {
        return "SizePolicy";
    }
};

// The ReplayPolicy performs only inlines specified by an external
// inline replay log.

class ReplayPolicy : public DiscretionaryPolicy
{
public:
    // Construct a ReplayPolicy
    ReplayPolicy(Compiler* compiler, bool isPrejitRoot);

    // Policy observations
    void NoteBool(InlineObservation obs, bool value) override;

    // Optional observations
    void NoteContext(InlineContext* context) override
    {
        m_InlineContext = context;
    }

    void NoteOffset(IL_OFFSETX offset) override
    {
        m_Offset = offset;
    }

    // Policy determinations
    void DetermineProfitability(CORINFO_METHOD_INFO* methodInfo) override;

    // Miscellaneous
    const char* GetName() const override
    {
        return "ReplayPolicy";
    }

    static void FinalizeXml();

private:
    bool FindMethod();
    bool FindContext(InlineContext* context);
    bool FindInline(CORINFO_METHOD_HANDLE callee);
    bool FindInline(unsigned token, unsigned hash, unsigned offset);

    static bool          s_WroteReplayBanner;
    static FILE*         s_ReplayFile;
    static CritSecObject s_XmlReaderLock;
    InlineContext*       m_InlineContext;
    IL_OFFSETX           m_Offset;
    bool                 m_WasForceInline;
};

#endif // defined(DEBUG) || defined(INLINE_DATA)

#endif // _INLINE_POLICY_H_
