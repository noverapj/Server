-- ============================================================================
-- game_individuality_get_self_index (queryId 2208)
-- SELECT latest idx for user after insert
-- Called by: DBClient::OnSelectIndividualityIndex (chained after insert)
-- Param: @accountIDX (user index)
-- Returns: idx (INT)
-- ============================================================================

IF OBJECT_ID(N'dbo.game_individuality_get_self_index', N'P') IS NOT NULL
    DROP PROCEDURE dbo.game_individuality_get_self_index;
GO

CREATE PROCEDURE dbo.game_individuality_get_self_index
    @accountIDX INT
AS
BEGIN
    SET NOCOUNT ON;

    SELECT TOP 1 idx
    FROM dbo.userIndividualityDB
    WHERE accountIDX = @accountIDX
    ORDER BY idx DESC;
END;
GO
