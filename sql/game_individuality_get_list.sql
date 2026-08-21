-- ============================================================================
-- game_individuality_get_list (queryId 2206)
-- SELECT all individuality data for user on login
-- Called by: DBClient::OnLoginSelectAllIndividuality
-- Param: @accountIDX (user index)
-- Returns: idx, ClassType, BasicTrait1-8, CoreTrait1-3 (per row)
-- regDate is NOT returned (DB-only field)
-- ============================================================================

IF OBJECT_ID(N'dbo.game_individuality_get_list', N'P') IS NOT NULL
    DROP PROCEDURE dbo.game_individuality_get_list;
GO

CREATE PROCEDURE dbo.game_individuality_get_list
    @accountIDX INT
AS
BEGIN
    SET NOCOUNT ON;

    SELECT
        idx,
        ClassType,
        BasicTrait1,
        BasicTrait2,
        BasicTrait3,
        BasicTrait4,
        BasicTrait5,
        BasicTrait6,
        BasicTrait7,
        BasicTrait8,
        CoreTrait1,
        CoreTrait2,
        CoreTrait3
    FROM dbo.userIndividualityDB
    WHERE accountIDX = @accountIDX;
END;
GO
