-- ============================================================================
-- game_individuality_save (queryId 2209)
-- UPDATE individuality data by idx
-- Called by: DBClient::OnUpdateIndividuality
-- Params: idx, ClassType, BasicTrait1-8, CoreTrait1-3
-- ============================================================================

IF OBJECT_ID(N'dbo.game_individuality_save', N'P') IS NOT NULL
    DROP PROCEDURE dbo.game_individuality_save;
GO

CREATE PROCEDURE dbo.game_individuality_save
    @idx         INT,
    @ClassType   INT,
    @BasicTrait1 INT = 0,
    @BasicTrait2 INT = 0,
    @BasicTrait3 INT = 0,
    @BasicTrait4 INT = 0,
    @BasicTrait5 INT = 0,
    @BasicTrait6 INT = 0,
    @BasicTrait7 INT = 0,
    @BasicTrait8 INT = 0,
    @CoreTrait1  INT = 0,
    @CoreTrait2  INT = 0,
    @CoreTrait3  INT = 0
AS
BEGIN
    SET NOCOUNT ON;

    UPDATE dbo.userIndividualityDB
    SET
        ClassType   = @ClassType,
        BasicTrait1 = @BasicTrait1,
        BasicTrait2 = @BasicTrait2,
        BasicTrait3 = @BasicTrait3,
        BasicTrait4 = @BasicTrait4,
        BasicTrait5 = @BasicTrait5,
        BasicTrait6 = @BasicTrait6,
        BasicTrait7 = @BasicTrait7,
        BasicTrait8 = @BasicTrait8,
        CoreTrait1  = @CoreTrait1,
        CoreTrait2  = @CoreTrait2,
        CoreTrait3  = @CoreTrait3
    WHERE idx = @idx;
END;
GO
